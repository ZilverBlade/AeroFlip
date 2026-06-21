// AeroFlip.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "AeroFlip.h"
#include "SWindowDrawObject.h"
#include "CD3D9ExRendererApi.h" 
#include "CWindowProvider.h" 
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <shlobj.h> 

#include <algorithm>
#include <sstream>

#define PROFILE_SCOPE(name) \
    struct ScopeTimer { \
        LARGE_INTEGER start, end, freq; \
        const wchar_t* label; \
        ScopeTimer(const wchar_t* l) : label(l) { \
            QueryPerformanceFrequency(&freq); \
            QueryPerformanceCounter(&start); \
																																																																																																																																																																																																																																																														        } \
        ~ScopeTimer() { \
            QueryPerformanceCounter(&end); \
            double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart; \
            std::wstringstream ss; \
            ss << L"[PROFILE] " << label << L": " << ms << L" ms\n"; \
            OutputDebugStringW(ss.str().c_str()); \
																																																																																																																																																																																																																																																														        } \
																																																																																																																															    } timer_##__LINE__(name)

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE g_hst = NULL;									// current instance
TCHAR g_szTitle[MAX_LOADSTRING];						// The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];					// the main window class name
HWINEVENTHOOK g_hEventHook = NULL;
HHOOK g_hKeyboardHook = NULL;
aeroflip::IRendererApi* g_pRenderer = NULL;				// Windowing renderer
aeroflip::CWindowProvider* g_pWindowProvider = NULL;	// DWMAPI Provider

// Global State Variables:
BOOL g_bWindowListDirty = TRUE;							// Tracks if the window list needs updating
UINT g_uActiveIndex = 0;									// Tracks which window index is front and center
LARGE_INTEGER g_liLastTime = { 0 };						// Stores the previous frame timestamp
double g_dFreq = 0.0;									// QPC Clock Frequency scalar
std::vector<aeroflip::SWindowDrawObject> g_DrawObjects;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE);
BOOL				InitInstance(HINSTANCE, int);
void				WakeAeroFlip(HWND);
void				DismissAeroFlip(HWND, HWND, BOOL);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK		WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
LRESULT CALLBACK	LowLevelKeyboardProc(int, WPARAM, LPARAM);

void				UpdateWindowAnimations(FLOAT);
void				MakeWindowTransparent(HWND);
void				InitializeWindowEventHook();
void				CleanupWindowEventHook();
void				FocusDesktopViaShell();

int APIENTRY _tWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	(void)::CreateMutex(NULL,
		TRUE,
		TEXT("Zilverblade_Aero_Flip_App_Mtx"));
	switch (::GetLastError()) {
	case ERROR_SUCCESS:
		// Process was not running already
		break;
	case ERROR_ALREADY_EXISTS:
		return TRUE;
	default:
		return FALSE;
	}

	LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_AEROFLIP, g_szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow)) return FALSE;

	HWND hWnd = FindWindow(g_szWindowClass, g_szTitle);

	{
		aeroflip::SWindowProviderConfig wConfig;
		ZeroMemory(&wConfig, sizeof(aeroflip::SWindowProviderConfig));

		wConfig.hWnd = hWnd;
		wConfig.szAppWindowClass = g_szWindowClass;

		g_pWindowProvider = new aeroflip::CWindowProvider(&wConfig);

		g_pWindowProvider->UpdateWindowList();
	}
	{
		aeroflip::SD3D9ExRendererApiConfig rConfig;
		ZeroMemory(&rConfig, sizeof(aeroflip::SD3D9ExRendererApiConfig));

		rConfig.hWnd = hWnd;
		rConfig.bHardwareAcceleration = TRUE;
		rConfig.uMultiSampleLevel = 4;

		g_pRenderer = new aeroflip::CD3D9ExRendererApi(&rConfig);
	}

	InitializeWindowEventHook();

	LARGE_INTEGER liFreq;
	QueryPerformanceFrequency(&liFreq);
	g_dFreq = 1.0 / double(liFreq.QuadPart);
	QueryPerformanceCounter(&g_liLastTime);

	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_HOTKEY && msg.wParam == 1)
			{
				HWND hLastActiveWindow = GetForegroundWindow();

				g_pWindowProvider->UpdateWindowList();
				if (hLastActiveWindow != hWnd)
				{
					g_pWindowProvider->FlagActiveWindow(hLastActiveWindow);
				}

				g_uActiveIndex = 0;
				WakeAeroFlip(hWnd);
			}
		}
		else
		{
			if (IsWindowVisible(hWnd))
			{
				bool bAltHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

				if (!bAltHeld)
				{
					HWND hTargetApp = NULL;
					BOOL bDesktop = FALSE;
					if (!g_DrawObjects.empty() && g_uActiveIndex < (UINT)g_DrawObjects.size())
					{
						hTargetApp = g_DrawObjects[g_uActiveIndex].hTargetWnd;
						bDesktop = g_DrawObjects[g_uActiveIndex].bDesktopBg;
					}
					else
					{
						hTargetApp = g_pWindowProvider->HGetActiveWindow();
					}

					DismissAeroFlip(hWnd, hTargetApp, bDesktop);
					GetAsyncKeyState(VK_MENU);
					continue;
				}

				if (g_bWindowListDirty)
				{
					PROFILE_SCOPE(L"Update Window List");

					g_pWindowProvider->UpdateWindowList();

					const aeroflip::SWindowTarget* pTargets = nullptr;
					UINT cTargets = 0;
					g_pWindowProvider->QueryWindows(&pTargets, &cTargets);

					std::vector<aeroflip::SWindowDrawObject> updatedObjects(cTargets);
					for (UINT i = 0; i < cTargets; ++i)
					{
						updatedObjects[i].hTargetWnd = pTargets[i].hWnd;
						updatedObjects[i].bFocused = pTargets[i].bActive;
						updatedObjects[i].bDesktopBg = pTargets[i].bDesktopWindow;

						// Retain spatial positions if this window existed previously
						BOOL bFoundOld = FALSE;
						for (const auto& oldObj : g_DrawObjects)
						{
							if (oldObj.hTargetWnd == pTargets[i].hWnd)
							{
								updatedObjects[i].fPosition[0] = oldObj.fPosition[0];
								updatedObjects[i].fPosition[1] = oldObj.fPosition[1];
								updatedObjects[i].fPosition[2] = oldObj.fPosition[2];
								updatedObjects[i].fRotationY = oldObj.fRotationY;
								updatedObjects[i].fOpacity = oldObj.fOpacity;
								bFoundOld = TRUE;
								break;
							}
						}

						updatedObjects[i].fPosition[0] = 0.0f;
						updatedObjects[i].fPosition[1] = 0.0f;
						updatedObjects[i].fPosition[2] = 0.1f;
						updatedObjects[i].fRotationY = 0.0f;
						updatedObjects[i].fOpacity = 1.0f;

						updatedObjects[i].fScale[0] = 1.2f;
						updatedObjects[i].fScale[1] = 1.2f;
						updatedObjects[i].fScale[2] = 1.0f;
					}
					g_DrawObjects = std::move(updatedObjects);
					g_bWindowListDirty = FALSE;
				}

				LARGE_INTEGER liCurrentTime;
				QueryPerformanceCounter(&liCurrentTime);
				float fDeltaTime = float(double(liCurrentTime.QuadPart - g_liLastTime.QuadPart) * g_dFreq);
				g_liLastTime = liCurrentTime;

				if (fDeltaTime > 0.1f) fDeltaTime = 0.1f;

				UpdateWindowAnimations(fDeltaTime);

				if (g_pRenderer)
				{
					g_pRenderer->UpdateWindows(g_pWindowProvider);
					// copy by value, to avoid window order state
					auto draws = g_DrawObjects;
					std::sort(draws.begin(), draws.end(),
						[](const aeroflip::SWindowDrawObject& a, const aeroflip::SWindowDrawObject& b)
					{
						return a.iZOrder < b.iZOrder;
					});
					g_pRenderer->OnRender(draws.data(), (UINT)draws.size());
				}
			}
			else
			{
				// Reset frame timestamps when inactive to skip processing gaps when woken back up
				QueryPerformanceCounter(&g_liLastTime);
				Sleep(10);
			}
		}
	}

	CleanupWindowEventHook();
	delete g_pRenderer;
	delete g_pWindowProvider;
	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AEROFLIP));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);

	wcex.hbrBackground = NULL;

	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_AEROFLIP);
	wcex.lpszClassName = g_szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	UNREFERENCED_PARAMETER(nCmdShow);

	g_hst = hInstance;

	HWND hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		g_szWindowClass,
		g_szTitle,
		WS_POPUP,
		0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, hInstance, NULL
		);

	if (!hWnd)
	{
		return FALSE;
	}

	if (!RegisterHotKey(hWnd, 1, MOD_CONTROL | MOD_WIN, VK_TAB))
	{
		OutputDebugString(L"Failed to register global shortcut!\n");
		return FALSE;
	}

	ShowWindow(hWnd, SW_HIDE);
	//ShowWindow(hWnd, nCmdShow);
	MakeWindowTransparent(hWnd);
	UpdateWindow(hWnd);

	return TRUE;
}

void WakeAeroFlip(HWND hWnd)
{
	if (g_pWindowProvider)
	{
		PROFILE_SCOPE(L"Invalidation");
		g_pWindowProvider->InvalidateAllWindows();
	}
	g_bWindowListDirty = TRUE; // Force an immediate refresh of the window targets
	ShowWindow(hWnd, SW_SHOW);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetForegroundWindow(hWnd);
}

void DismissAeroFlip(HWND hWnd, HWND hSelectedApp, BOOL bDesktopBackground)
{
	ShowWindow(hWnd, SW_HIDE);
	// reduce memory usage when dismissed
	if (g_pRenderer != NULL)
	{
		g_pRenderer->ReleaseWindows();
	}
	// clear up RAM
	SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

	if (bDesktopBackground)
	{
		FocusDesktopViaShell();
	}
	else if (hSelectedApp != NULL)
	{
		if (IsIconic(hSelectedApp))
		{
			ShowWindow(hSelectedApp, SW_RESTORE);
		}
		else
		{
			HWND hForeground = GetForegroundWindow();
			if (hForeground != hSelectedApp)
			{
				SetForegroundWindow(hSelectedApp);
			}
		}
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;

	switch (message)
	{
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		UNREFERENCED_PARAMETER(wmId);
		UNREFERENCED_PARAMETER(wmEvent);

		return DefWindowProc(hWnd, message, wParam, lParam);
		break;

	case WM_PAINT:
		ValidateRect(hWnd, NULL);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void MakeWindowTransparent(HWND hWnd)
{
	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hWnd, &margins);
}

void CALLBACK WinEventProc(
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hWnd,
	LONG idObject,
	LONG idChild,
	DWORD dwEventThread,
	DWORD dwmsEventTime)
{

	UNREFERENCED_PARAMETER(hWinEventHook);
	UNREFERENCED_PARAMETER(dwEventThread);
	UNREFERENCED_PARAMETER(dwmsEventTime);

	if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

	switch (event) {
	case EVENT_SYSTEM_FOREGROUND:
		if (g_pWindowProvider)
		{
			// Update immediately so cache can occur
			g_pWindowProvider->UpdateWindowList();
			if (!IsIconic(hWnd))
			{
				PROFILE_SCOPE(L"Cache Foreground Window");
				g_pWindowProvider->CacheWindowThumbnail(hWnd);
			}

			g_pWindowProvider->FlagActiveWindow(hWnd);
		}
		break;
	case EVENT_OBJECT_DESTROY:
		g_bWindowListDirty = TRUE; // A window closed, we must rebuild the structure
		g_pWindowProvider->ClearWindowCache(hWnd);
		break;

	case EVENT_SYSTEM_MOVESIZEEND:
		g_bWindowListDirty = TRUE;
		if (g_pWindowProvider)
		{
			PROFILE_SCOPE(L"Resize Window");
			g_pWindowProvider->InvalidateWindow(hWnd);
		}
		break;

	case EVENT_SYSTEM_MINIMIZESTART:
		g_bWindowListDirty = TRUE;
		if (g_pWindowProvider)
		{
			PROFILE_SCOPE(L"Minimise Window");

			g_pWindowProvider->SetWindowMinimizedFlag(hWnd, TRUE);
		}
		break;

	case EVENT_SYSTEM_MINIMIZEEND:
		g_bWindowListDirty = TRUE;
		if (g_pWindowProvider)
		{
			PROFILE_SCOPE(L"Restore Window");
			g_pWindowProvider->InvalidateWindow(hWnd);
			g_pWindowProvider->SetWindowMinimizedFlag(hWnd, FALSE);
		}
		break;
	}
}
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;

		if (pKeyStruct->vkCode == VK_TAB)
		{
			bool bAltPressed = (pKeyStruct->flags & LLKHF_ALTDOWN) != 0;

			if (bAltPressed)
			{
				if (wParam == WM_SYSKEYDOWN)
				{
					HWND hWnd = FindWindow(g_szWindowClass, g_szTitle);

					if (!IsWindowVisible(hWnd))
					{
						g_bWindowListDirty = TRUE;
						HWND hLastActiveWindow = GetForegroundWindow();
						if (hLastActiveWindow != hWnd)
						{
							g_pWindowProvider->FlagActiveWindow(hLastActiveWindow);
						}

						g_uActiveIndex = 0;
						WakeAeroFlip(hWnd);
					}
					else
					{
						// Cycle target indexing forwards
						if (!g_DrawObjects.empty())
						{
							g_uActiveIndex = (g_uActiveIndex + 1) % (UINT)g_DrawObjects.size();
						}
					}
					return 1;
				}

				if (wParam == WM_SYSKEYUP)
				{
					return 1;
				}
			}
		}
	}

	return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

void UpdateWindowAnimations(FLOAT fDeltaTime)
{
	INT iNumWindows = (INT)g_DrawObjects.size();
	if (iNumWindows == 0) return;

	const UINT uMaxShowWindows = 10;

	FLOAT fLerpFactor = 12.0f * fDeltaTime;
	if (fLerpFactor > 1.0f) fLerpFactor = 1.0f;

	for (INT i = 0; i < iNumWindows; ++i)
	{
		auto& window = g_DrawObjects[i];
		INT iRelativeIndex = (i - (INT)g_uActiveIndex + iNumWindows) % iNumWindows;

		FLOAT fTargetX = 1.0f - (iRelativeIndex * 1.2f);
		FLOAT fTargetY = -0.2f + (iRelativeIndex * 0.2f);
		FLOAT fTargetZ = 3.0f + (iRelativeIndex * 1.5f);
		FLOAT fTargetRotY = -50.0f;
		FLOAT fTargetOpacity = 1.0f;

		if (iRelativeIndex == iNumWindows - 1 && iNumWindows > 1)
		{
			if (window.iZOrder != iRelativeIndex)
			{
				window.bMovingToBack = TRUE;
			}
			if (window.fOpacity > 0.05f && window.bMovingToBack)
			{
				fTargetX = 2.5f;
				fTargetY = -0.8f;
				fTargetZ = 1.5f;
				fTargetOpacity = 0.0f;
				window.iZOrder = -1;
			}
			else
			{
				INT backIndex = min(iNumWindows - 1, (INT)uMaxShowWindows);
				fTargetX = 1.0f - (backIndex * 1.2f);
				fTargetY = -0.2f + (backIndex * 0.2f);
				fTargetZ = 3.0f + (backIndex * 1.5f);
				fTargetOpacity = 1.0f;
				window.iZOrder = backIndex;
				window.bMovingToBack = FALSE;
				if (window.fPosition[0] > 1.5f)
				{
					window.fPosition[0] = fTargetX;
					window.fPosition[1] = fTargetY;
					window.fPosition[2] = fTargetZ;
				}
			}
		}
		else
		{
			window.iZOrder = iRelativeIndex;

			if (iRelativeIndex >= (INT)uMaxShowWindows)
			{
				fTargetOpacity = 0.0f;
				fTargetX = 1.0f - (uMaxShowWindows * 1.2f);
				fTargetY = -0.2f + (uMaxShowWindows * 0.2f);
				fTargetZ = 3.0f + (uMaxShowWindows * 1.5f);
			}
			else if (iRelativeIndex >= (INT)uMaxShowWindows - 2)
			{
				fTargetOpacity = 1.0f - ((iRelativeIndex - ((INT)uMaxShowWindows - 2)) * 0.5f);
			}
		}

		window.fPosition[0] += (fTargetX - window.fPosition[0]) * fLerpFactor;
		window.fPosition[1] += (fTargetY - window.fPosition[1]) * fLerpFactor;
		window.fPosition[2] += (fTargetZ - window.fPosition[2]) * fLerpFactor;
		window.fRotationY += (fTargetRotY - window.fRotationY) * fLerpFactor;
		window.fOpacity += (fTargetOpacity - window.fOpacity) * fLerpFactor;
	}
}

void InitializeWindowEventHook()
{
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
	if (!g_hKeyboardHook) {
		OutputDebugString(L"Failed to install Low-Level Keyboard Hook!\n");
	}

	g_hEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_DESTROY,
		NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
}

void CleanupWindowEventHook()
{
	if (g_hKeyboardHook) {
		UnhookWindowsHookEx(g_hKeyboardHook);
	}

	if (g_hEventHook) {
		UnhookWinEvent(g_hEventHook);
	}
}


void FocusDesktopViaShell()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	IShellDispatch4* pShellDispatch = NULL;
	hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pShellDispatch));

	if (SUCCEEDED(hr) && pShellDispatch)
	{
		pShellDispatch->ToggleDesktop();
		pShellDispatch->Release();
	}
	else
	{
		HWND hShell = FindWindow(L"Shell_TrayWnd", NULL);
		if (hShell)
		{
			SendMessage(hShell, WM_COMMAND, 419, 0); // 419 is the ID for 'Minimize All Windows'
		}
	}

	if (SUCCEEDED(hr))
	{
		CoUninitialize();
	}
}