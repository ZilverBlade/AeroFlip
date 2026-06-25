// AeroFlip.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "AeroFlip.h"
#include "SAeroFlipConfig.h"
#include "SWindowDrawObject.h"
#include "CD3D9ExRendererApi.h" 
#include "CWindowProvider.h" 
#include "CommonsCfg.h"
#include "DwmLivePreview.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <shlobj.h> 
#include <shellapi.h>

#include <algorithm>
#include <sstream>

#define PROFILE_SCOPE(name) \
    struct ScopeTimer { \
        LARGE_INTEGER start, end, freq; \
        const wchar_t* label; \
        ScopeTimer(const wchar_t* l) : label(l) { \
            QueryPerformanceFrequency(&freq); \
            QueryPerformanceCounter(&start); } \
        ~ScopeTimer() { \
            QueryPerformanceCounter(&end); \
            double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart; \
            std::wstringstream ss; \
            ss << L"[PROFILE] " << label << L": " << ms << L" ms\n"; \
            OutputDebugStringW(ss.str().c_str()); }}

#define MAX_LOADSTRING 100

#define WM_TRAYICON         (WM_USER + 1)
#define ID_TRAY_HIDE        3001
#define ID_TRAY_STOP        3002
#define ID_TRAY_CONFIG      3003
#define UID_NOTIF_TRAY		1;

static FLOAT fmix(FLOAT fStart, FLOAT fEnd, FLOAT fAlpha)
{
	return fStart * (1.0f - fAlpha) + fEnd * fAlpha;
}

// Global Variables:
HINSTANCE g_hst = NULL;									// current instance
TCHAR g_szTitle[MAX_LOADSTRING];						// The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];					// the main window class name
HWINEVENTHOOK g_hEventHook = NULL;
HHOOK g_hKeyboardHook = NULL;
aeroflip::SAeroFlipConfig g_AeroFlipCfg;
aeroflip::IRendererApi* g_pRenderer = NULL;				// Windowing renderer
aeroflip::CWindowProvider* g_pWindowProvider = NULL;	// DWMAPI Provider

// Global State Variables for File Watching
HANDLE g_hWatcherThread = NULL;
HANDLE g_hWatcherStopEvent = NULL;
CRITICAL_SECTION g_csRendererLock;
BOOL g_bRecreateRenderer = FALSE;

// Global State Variables:
BOOL g_bWindowListDirty = TRUE;							// Tracks if the window list needs updating
BOOL g_bIsDismissing = FALSE;							// Tracks if AeroFlip has released Alt+Tab
BOOL g_bIsCycling = FALSE;								// Tracks if alt+tab cycle is happening now
UINT g_uActiveIndex = 0;								// Tracks which window index is front and center
HWND g_hLastActiveWindow = NULL;						// Tracks which window was active right before AeroFlip
MONITORINFO g_miLastActiveMonitor;
FLOAT g_fDesktopDimmingFactor = 0.0f;
FLOAT g_vMonitorShift[2] = { 0 };
LARGE_INTEGER g_liLastTime = { 0 };						// Stores the previous frame timestamp
DOUBLE g_dFreq = 0.0;									// QPC Clock Frequency scalar
std::vector<aeroflip::SWindowDrawObject> g_DrawObjects;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE);
BOOL				InitInstance(HINSTANCE, int);
void				AddTrayIcon(HWND);
void				RemoveTrayIcon(HWND);
void				ShowTrayContextMenu(HWND);
void				OpenConfigurator(HWND);

void				WakeAeroFlip(HWND);
void				DismissAeroFlip(HWND, HWND, BOOL);
void				UpdateDrawObjects();
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK		WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
LRESULT CALLBACK	LowLevelKeyboardProc(int, WPARAM, LPARAM);
DWORD WINAPI		ConfigFileWatcherThread(LPVOID);

void                LoadConfig();

void				UpdateWindowAnimations(FLOAT);
void				MakeWindowTransparent(HWND);
void				InitializeWindowEventHook();
void				CleanupWindowEventHook();
void				FocusDesktopViaShell();
//void				HideWindowsFromView(HWND);
//void				RestoreWindowsFromView(HWND);

aeroflip::IRendererApi* GetRendererApi(const aeroflip::SRendererConfig* pRendererConfig, HWND hWnd)
{
	return new aeroflip::CD3D9ExRendererApi(pRendererConfig, hWnd);
}

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
		OutputDebugString(TEXT("App instance already running\n"));
		return TRUE;
	default:
		OutputDebugString(TEXT("Unknown error occurred creating mutex, terminating...\n"));
		return FALSE;
	}

	LoadConfig();

	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));
	try
	{
		LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
		LoadString(hInstance, IDC_AEROFLIP, g_szWindowClass, MAX_LOADSTRING);
		MyRegisterClass(hInstance);

		if (!InitInstance(hInstance, nCmdShow))
		{
			return FALSE;
		}

		HWND hWnd = FindWindow(g_szWindowClass, g_szTitle);

		g_pWindowProvider = new aeroflip::CWindowProvider(hWnd, g_szWindowClass);
		g_pWindowProvider->UpdateWindowList();

		g_pRenderer = GetRendererApi(&g_AeroFlipCfg.rConfig, hWnd);

		InitializeWindowEventHook();
		InitializeCriticalSection(&g_csRendererLock);

		LARGE_INTEGER liFreq;
		QueryPerformanceFrequency(&liFreq);
		g_dFreq = 1.0 / double(liFreq.QuadPart);
		QueryPerformanceCounter(&g_liLastTime);

		g_hWatcherStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		g_hWatcherThread = CreateThread(NULL, 0, ConfigFileWatcherThread, hWnd, 0, NULL);

		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				if (IsWindowVisible(hWnd))
				{
					BOOL bDismissTriggered = FALSE;

					if (g_AeroFlipCfg.kbConfig.dwFlipShortcutMode == aeroflip::eFSM_WIN_TAB)
					{
						// In Win+Tab mode, dismissal is handled directly in LowLevelKeyboardProc asynchronously
						bDismissTriggered = g_bIsDismissing;
					}
					else
					{
						BOOL bAltHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

						if (!bAltHeld)
						{
							if (!g_bIsDismissing)
							{
								g_bIsDismissing = TRUE;
							}
							bDismissTriggered = TRUE;
						}
						else
						{
							g_bIsDismissing = FALSE;
						}
					}

					if (bDismissTriggered)
					{
						BOOL bDismissComplete = FALSE;
						if (g_DrawObjects.empty() || g_uActiveIndex >= (UINT)g_DrawObjects.size())
						{
							bDismissComplete = TRUE;
						}
						else
						{
							if (g_DrawObjects[g_uActiveIndex].fPosition[2] <= 0.11f)
							{
								bDismissComplete = TRUE;
							}
						}

						if (bDismissComplete)
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
							g_bIsDismissing = FALSE;

							if (g_AeroFlipCfg.kbConfig.dwFlipShortcutMode != aeroflip::eFSM_WIN_TAB)
							{
								GetAsyncKeyState(VK_MENU);
							}
							continue;
						}
					}

					if (g_bWindowListDirty)
					{
						PROFILE_SCOPE(L"Update Window List");

						g_pWindowProvider->UpdateWindowList();
						UpdateDrawObjects();
						g_bWindowListDirty = FALSE;

						QueryPerformanceCounter(&g_liLastTime);
					}

					LARGE_INTEGER liCurrentTime;
					QueryPerformanceCounter(&liCurrentTime);
					float fDeltaTime = float(double(liCurrentTime.QuadPart - g_liLastTime.QuadPart) * g_dFreq);
					g_liLastTime = liCurrentTime;

					if (fDeltaTime > 0.1f) fDeltaTime = 0.1f;

					UpdateWindowAnimations(fDeltaTime);

					EnterCriticalSection(&g_csRendererLock);
					if (g_bRecreateRenderer)
					{
						delete g_pRenderer;
						g_pRenderer = GetRendererApi(&g_AeroFlipCfg.rConfig, hWnd);
						g_bRecreateRenderer = FALSE;
					}
					LeaveCriticalSection(&g_csRendererLock);

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
						
						FLOAT fBgOpacity = g_fDesktopDimmingFactor * g_AeroFlipCfg.sConfig.uDesktopDimmingPercent / 100.0f;
						if (g_AeroFlipCfg.sConfig.bShowDesktopWhenFlipping)
						{
							fBgOpacity = fmix(1.0f, g_AeroFlipCfg.sConfig.uDesktopDimmingPercent / 100.0f, g_fDesktopDimmingFactor);
						}
						g_pRenderer->OnRender(draws.data(),
							(UINT)draws.size(),
							g_AeroFlipCfg.sConfig.bShowDesktopWhenFlipping,
							fBgOpacity,
							g_vMonitorShift
							);
					}
				}
				else
				{
					Sleep(10);
					// Reset frame timestamps when inactive to skip processing gaps when woken back up
					QueryPerformanceCounter(&g_liLastTime);
				}
			}
		}
	}
	catch (std::exception ex)
	{
		OutputDebugString(TEXT("Exception thrown! "));
		OutputDebugStringA(ex.what());
		OutputDebugString(TEXT("\n"));
	}

	if (g_hWatcherStopEvent)
	{
		SetEvent(g_hWatcherStopEvent);
		WaitForSingleObject(g_hWatcherThread, 2000); // wait up to 2s before giving up
		CloseHandle(g_hWatcherThread);
		CloseHandle(g_hWatcherStopEvent);
	}
	DeleteCriticalSection(&g_csRendererLock);

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

	INT vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
	INT vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
	INT vcx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	INT vcy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	HWND hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		g_szWindowClass,
		g_szTitle,
		WS_POPUP,
		vx, vy, vcx, vcy,
		NULL, NULL, hInstance, NULL
		);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, SW_HIDE);
	MakeWindowTransparent(hWnd);
	UpdateWindow(hWnd);
	AddTrayIcon(hWnd);

	// prevent hiding when peeking
	BOOL policy = TRUE;
	DwmSetWindowAttribute(hWnd, DWMWA_EXCLUDED_FROM_PEEK, &policy, sizeof(BOOL));
	DwmSetWindowAttribute(hWnd, DWMWA_DISALLOW_PEEK, &policy, sizeof(BOOL));
	DwmSetWindowAttribute(hWnd, DWMWA_TRANSITIONS_FORCEDISABLED, &policy, sizeof(BOOL));

	return TRUE;
}

void AddTrayIcon(HWND hWnd)
{
	NOTIFYICONDATA nid = { 0 };
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = UID_NOTIF_TRAY;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;

	nid.hIcon = LoadIcon(g_hst, MAKEINTRESOURCE(IDI_AEROFLIP));
	if (!nid.hIcon)
	{
		OutputDebugString(TEXT("Failed to setup tray icon\n"));
		return;
	}

	lstrcpyn(nid.szTip, TEXT("AeroFlip"), sizeof(nid.szTip) / sizeof(TCHAR));

	Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd)
{
	NOTIFYICONDATA nid = { 0 };
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = UID_NOTIF_TRAY;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowTrayContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);

	HMENU hMenu = CreatePopupMenu();
	if (hMenu)
	{
		InsertMenu(hMenu, UINT_MAX, MF_BYPOSITION | MF_STRING, ID_TRAY_CONFIG, L"Open Configurator");
		InsertMenu(hMenu, UINT_MAX, MF_BYPOSITION | MF_STRING, ID_TRAY_HIDE, L"Hide Icon");
		InsertMenu(hMenu, UINT_MAX, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
		InsertMenu(hMenu, UINT_MAX, MF_BYPOSITION | MF_STRING, ID_TRAY_STOP, L"Stop AeroFlip");

		SetForegroundWindow(hWnd);

		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
		DestroyMenu(hMenu);
	}
}

void OpenConfigurator(HWND hWnd)
{
	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);

	TCHAR* lastSlash = _tcsrchr(exePath, L'\\');
	if (lastSlash != NULL)
	{
		*(lastSlash + 1) = TEXT('\0');
	}

	lstrcat(exePath, TEXT("AeroFlipConfigurator.exe"));

	HINSTANCE hInst = ShellExecute(
		hWnd,
		L"open",
		exePath,
		NULL,
		exePath,
		SW_SHOW
		);

	if ((INT_PTR)hInst <= 32)
	{
		OutputDebugString(TEXT("Failed to launch AeroFlipConfigurator.exe"));
	}
}

void WakeAeroFlip(HWND hWnd)
{
	//if (g_AeroFlipCfg.sConfig.bShowDesktopWhenFlipping)
	//{
	//	HideWindowsFromView(hWnd);
	//}
	if (g_pWindowProvider)
	{
		PROFILE_SCOPE(L"Invalidation");
		g_pWindowProvider->InvalidateAllWindows();
	}
	g_bWindowListDirty = TRUE; // Force an immediate refresh of the window targets
	g_DrawObjects.clear();
	ShowWindow(hWnd, SW_SHOW);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetForegroundWindow(hWnd);
	if (g_AeroFlipCfg.kbConfig.bCycleOnFirstTab)
	{
		// initial tab requires draw objects to be visible to switch
		if (g_pWindowProvider)
		{
			g_pWindowProvider->UpdateWindowList();
			UpdateDrawObjects();
		}
	}
	g_fDesktopDimmingFactor = 1.0f; // no dim yet
}

void DismissAeroFlip(HWND hWnd, HWND hSelectedApp, BOOL bDesktopBackground)
{
	ShowWindow(hWnd, SW_HIDE);

	// reduce memory usage when dismissed
	if (g_pRenderer != NULL)
	{
		if (!g_AeroFlipCfg.rConfig.bPersistInVRAM)
		{
			g_pRenderer->ReleaseWindows();
		}
	}

	// clear up RAM	
	SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

	//// Restore regardless of config, might have changed while flipping was active!
	//RestoreWindowsFromView(hWnd);

	if (bDesktopBackground)
	{
		OutputDebugString(TEXT("Focussing Desktop\n"));
		FocusDesktopViaShell();
	}
	else if (hSelectedApp != NULL)
	{
		if (IsIconic(hSelectedApp))
		{
			// Disable animation temporarily
			// Modified from https://stackoverflow.com/a/6088475

			ANIMATIONINFO oldAnimInfo;
			oldAnimInfo.cbSize = sizeof(ANIMATIONINFO);

			BOOL bSuppressed = FALSE;

			if (::SystemParametersInfo(SPI_GETANIMATION,
				sizeof(ANIMATIONINFO),
				&oldAnimInfo, 0)) {
				ANIMATIONINFO noAnimation;
				ZeroMemory(&noAnimation, sizeof(ANIMATIONINFO));
				noAnimation.cbSize = sizeof(ANIMATIONINFO);
				::SystemParametersInfo(SPI_SETANIMATION,
					sizeof(ANIMATIONINFO), &noAnimation,
					SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
				bSuppressed = TRUE;
			}

			ShowWindow(hSelectedApp, SW_RESTORE);
			SetForegroundWindow(hSelectedApp);

			if (bSuppressed) {
				::SystemParametersInfo(SPI_SETANIMATION,
					sizeof(ANIMATIONINFO),
					&oldAnimInfo,
					SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
			}
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

void UpdateDrawObjects()
{
	const aeroflip::SWindowTarget* pTargets = nullptr;
	UINT cTargets = 0;
	g_pWindowProvider->QueryWindows(&pTargets, &cTargets);

	std::vector<aeroflip::SWindowDrawObject> updatedObjects(cTargets);
	for (UINT i = 0; i < cTargets; ++i)
	{
		updatedObjects[i].hTargetWnd = pTargets[i].hWnd;
		updatedObjects[i].bFocused = pTargets[i].bActive;
		updatedObjects[i].bDesktopBg = pTargets[i].bDesktopWindow;
		updatedObjects[i].bDecorated = g_AeroFlipCfg.sConfig.dwWindowFrameStyle == aeroflip::eWFS_WINDOWS_AERO
			&& pTargets[i].bDecorated;

		RECT r;
		ZeroMemory(&r, sizeof(RECT));
		if (IsIconic(pTargets[i].hWnd))
		{
			if (pTargets[i].bHasCachedBounds)
			{
				r = pTargets[i].rcCachedBounds;
			}
			else
			{
				WINDOWPLACEMENT wp;
				wp.length = sizeof(WINDOWPLACEMENT);
				GetWindowPlacement(pTargets[i].hWnd, &wp);
				r = wp.rcNormalPosition;
			}
		}
		else
		{
			GetClientRect(pTargets[i].hWnd, &r);
			ClientToScreen(pTargets[i].hWnd, (POINT*)&r.left);
			ClientToScreen(pTargets[i].hWnd, (POINT*)&r.right);
		}
		updatedObjects[i].rcBounds = r;

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
				updatedObjects[i].fScale[0] = oldObj.fScale[0];
				updatedObjects[i].fScale[1] = oldObj.fScale[1];
				updatedObjects[i].fScale[2] = oldObj.fScale[2];
				updatedObjects[i].dwMoveMode = aeroflip::eWDOMM_DEFAULT;
				bFoundOld = TRUE;
				break;
			}
		}

		if (!bFoundOld)
		{
			FLOAT vx = (FLOAT)GetSystemMetrics(SM_XVIRTUALSCREEN);
			FLOAT vy = (FLOAT)GetSystemMetrics(SM_YVIRTUALSCREEN);
			FLOAT Sw = (FLOAT)GetSystemMetrics(SM_CXVIRTUALSCREEN);
			FLOAT Sh = (FLOAT)GetSystemMetrics(SM_CYVIRTUALSCREEN);

			FLOAT H_frust = 4.224978f;
			FLOAT W_frust = H_frust * (Sw / Sh);

			FLOAT w = (FLOAT)(r.right - r.left);
			if (w <= 0)
			{
				w = 100.0f;
			}
			FLOAT h = (FLOAT)(r.bottom - r.top);
			if (h <= 0)
			{
				h = 100.0f;
			}
			FLOAT cx = r.left + w / 2.0f;
			FLOAT cy = r.top + h / 2.0f;

			updatedObjects[i].fPosition[0] = (((cx - vx) / Sw) * 2.0f - 1.0f) * (W_frust / 2.0f);
			updatedObjects[i].fPosition[1] = (1.0f - ((cy - vy) / Sh) * 2.0f) * (H_frust / 2.0f);
			updatedObjects[i].fPosition[2] = 0.1f;
			updatedObjects[i].fRotationY = 0.0f;
			updatedObjects[i].fOpacity = 1.0f;

			FLOAT matchScale = (H_frust / Sh) * (h / 2.0f);
			updatedObjects[i].fScale[0] = matchScale;
			updatedObjects[i].fScale[1] = matchScale;
			updatedObjects[i].fScale[2] = 1.0f;
		}
	}
	g_DrawObjects = std::move(updatedObjects);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;

	switch (message)
	{
	case WM_DISPLAYCHANGE:
	{
		INT vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
		INT vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
		INT vcx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		INT vcy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		MoveWindow(hWnd, vx, vy, vcx, vcy, TRUE);

		EnterCriticalSection(&g_csRendererLock);
		g_bRecreateRenderer = TRUE;
		LeaveCriticalSection(&g_csRendererLock);

		g_bWindowListDirty = TRUE;
	}
	break;
	case WM_TRAYICON:
		if (LOWORD(lParam) == WM_RBUTTONUP)
		{
			ShowTrayContextMenu(hWnd);
			return 0;
		}
		if (LOWORD(lParam) == WM_LBUTTONDOWN)
		{
			OpenConfigurator(hWnd);
			return 0;
		}
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmId)
		{
		case ID_TRAY_HIDE:
			RemoveTrayIcon(hWnd);
			break;

		case ID_TRAY_STOP:
			RemoveTrayIcon(hWnd);
			PostQuitMessage(0);
			break;

		case ID_TRAY_CONFIG:
			OpenConfigurator(hWnd);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:
		ValidateRect(hWnd, NULL);
		break;

	case WM_DESTROY:
		RemoveTrayIcon(hWnd);
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
			g_bWindowListDirty = TRUE;
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

void FlipWindow()
{
	if (!g_DrawObjects.empty())
	{
		bool bShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		if (bShiftPressed && g_AeroFlipCfg.kbConfig.bShiftToMoveBack)
		{
			g_uActiveIndex = (g_uActiveIndex + g_DrawObjects.size() - 1) % (UINT)g_DrawObjects.size();
		}
		else
		{
			g_uActiveIndex = (g_uActiveIndex + 1) % (UINT)g_DrawObjects.size();
		}
		g_bIsCycling = TRUE;
	}
}

void TriggerAeroFlipActivation(HWND hWnd)
{
	g_bWindowListDirty = TRUE;
	g_hLastActiveWindow = GetForegroundWindow();

	if (g_hLastActiveWindow != hWnd)
	{
		g_pWindowProvider->FlagActiveWindow(g_hLastActiveWindow);
	}

	UINT cWindows = 0;
	const aeroflip::SWindowTarget* pWindows = NULL;

	g_pWindowProvider->QueryWindows(&pWindows, &cWindows);

	BOOL bFoundActiveWindow = FALSE;
	INT iDesktopIndex = -1;

	for (UINT i = 0; i < cWindows; ++i)
	{
		if (pWindows[i].bDesktopWindow)
		{
			iDesktopIndex = i;
		}
		else if (pWindows[i].hWnd == g_hLastActiveWindow)
		{
			g_uActiveIndex = i;
			bFoundActiveWindow = TRUE;
			break;
		}
	}

	if (!bFoundActiveWindow && iDesktopIndex != -1)
	{
		g_uActiveIndex = (UINT)iDesktopIndex;
		g_hLastActiveWindow = NULL;
	}
	else if (!bFoundActiveWindow && cWindows > 0)
	{
		g_uActiveIndex = 0;
		g_hLastActiveWindow = pWindows[0].hWnd;
	}

	HMONITOR hMon = MonitorFromWindow(g_hLastActiveWindow, MONITOR_DEFAULTTOPRIMARY);
	g_miLastActiveMonitor = { sizeof(MONITORINFO) };
	GetMonitorInfo(hMon, &g_miLastActiveMonitor);

	WakeAeroFlip(hWnd);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;
		HWND hWnd = FindWindow(g_szWindowClass, g_szTitle);
		BOOL bAeroFlipActive = IsWindowVisible(hWnd);

		if (g_AeroFlipCfg.kbConfig.dwFlipShortcutMode == aeroflip::eFSM_WIN_TAB)
		{
			BOOL bWinDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

			if (pKeyStruct->vkCode == VK_TAB && bAeroFlipActive)
			{
				if (wParam == WM_KEYDOWN)
				{
					FlipWindow();
				}
				return 1; // Suppress built-in system task cycling
			}

			if (pKeyStruct->vkCode == VK_TAB && bWinDown && !bAeroFlipActive)
			{
				if (wParam == WM_KEYDOWN)
				{
					INPUT input = { 0 };
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_NONAME;
					SendInput(1, &input, sizeof(INPUT));

					TriggerAeroFlipActivation(hWnd);

					if (g_AeroFlipCfg.kbConfig.bCycleOnFirstTab)
					{
						FlipWindow();
					}
				}
				return 1;
			}

			// Trap Windows keys to dismiss if window is open
			if (pKeyStruct->vkCode == VK_LWIN || pKeyStruct->vkCode == VK_RWIN)
			{
				if (bAeroFlipActive)
				{
					if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
					{
						return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
					}

					if (wParam == WM_KEYDOWN)
					{
						g_bIsDismissing = !g_bIsDismissing;
						return 1;
					}
				}
			}
		}
		else if (g_AeroFlipCfg.kbConfig.dwFlipShortcutMode == aeroflip::eFSM_ALT_TAB)
		{
			if (pKeyStruct->vkCode == VK_TAB)
			{
				BOOL bAltPressed = (pKeyStruct->flags & LLKHF_ALTDOWN) != 0;
				if (bAltPressed)
				{
					if (wParam == WM_SYSKEYDOWN)
					{
						if (!g_bIsDismissing)
						{
							BOOL bSecondTab = FALSE;
							if (!bAeroFlipActive)
							{
								TriggerAeroFlipActivation(hWnd);
							}
							else
							{
								bSecondTab = TRUE;
							}

							if (g_AeroFlipCfg.kbConfig.bCycleOnFirstTab || bSecondTab)
							{
								FlipWindow();
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

			// prevent keys from being sticky
			if ((pKeyStruct->vkCode == VK_LMENU || pKeyStruct->vkCode == VK_RMENU || pKeyStruct->vkCode == VK_MENU))
			{
				if (bAeroFlipActive && (wParam == WM_KEYUP || wParam == WM_SYSKEYUP))
				{
					g_bIsDismissing = TRUE;
					return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
				}
			}
		}
	}

	return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

DWORD WINAPI ConfigFileWatcherThread(LPVOID lpParam)
{
	HWND hWndMain = (HWND)lpParam;
	UNREFERENCED_PARAMETER(hWndMain);

	std::wstring fullPath = aeroflip::GetConfigIniPath();
	size_t lastSlash = fullPath.find_last_of(L"\\/");
	if (lastSlash == std::wstring::npos) return 0;

	std::wstring directory = fullPath.substr(0, lastSlash);
	std::wstring fileName = fullPath.substr(lastSlash + 1);

	HANDLE hNotification = FindFirstChangeNotificationW(
		directory.c_str(),
		FALSE,
		FILE_NOTIFY_CHANGE_LAST_WRITE
		);

	if (hNotification == INVALID_HANDLE_VALUE) return 0;

	HANDLE waitHandles[2] = { g_hWatcherStopEvent, hNotification };

	while (TRUE)
	{
		DWORD dwWaitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

		if (dwWaitResult == WAIT_OBJECT_0)
		{
			// Stop event was signaled
			break;
		}
		else if (dwWaitResult == WAIT_OBJECT_0 + 1)
		{
			// Note: A small sleep handles cases where the writing process locks the file momentarily
			Sleep(100);

			LoadConfig();

			EnterCriticalSection(&g_csRendererLock);
			g_bRecreateRenderer = TRUE;
			LeaveCriticalSection(&g_csRendererLock);

			OutputDebugString(L"[Watcher] Config file updated. Renderer successfully recreated.\n");

			FindNextChangeNotification(hNotification);
		}
	}

	FindCloseChangeNotification(hNotification);
	return 0;
}

void LoadConfig()
{
	g_AeroFlipCfg.kbConfig.InitDefault();
	g_AeroFlipCfg.rConfig.InitDefault();
	g_AeroFlipCfg.sConfig.InitDefault();

	DefineIni(IniRead, &g_AeroFlipCfg);
}

void UpdateWindowAnimations(FLOAT fDeltaTime)
{
	INT iNumWindows = (INT)g_DrawObjects.size();
	if (iNumWindows == 0)
	{
		return;
	}

	UINT uMaxShowWindows = g_AeroFlipCfg.sConfig.uMaxWindowsVisible;

	FLOAT fLerpFactor = min(g_AeroFlipCfg.sConfig.iAnimationSpeed * fDeltaTime, 1.0f);

	FLOAT vx = (FLOAT)GetSystemMetrics(SM_XVIRTUALSCREEN);
	FLOAT vy = (FLOAT)GetSystemMetrics(SM_YVIRTUALSCREEN);
	FLOAT Sw = (FLOAT)GetSystemMetrics(SM_CXVIRTUALSCREEN);
	FLOAT Sh = (FLOAT)GetSystemMetrics(SM_CYVIRTUALSCREEN);

	FLOAT mx = (FLOAT)g_miLastActiveMonitor.rcMonitor.left;
	FLOAT my = (FLOAT)g_miLastActiveMonitor.rcMonitor.top;
	FLOAT mcx = (FLOAT)g_miLastActiveMonitor.rcMonitor.right - g_miLastActiveMonitor.rcMonitor.left;
	FLOAT mcy = (FLOAT)g_miLastActiveMonitor.rcMonitor.bottom - g_miLastActiveMonitor.rcMonitor.top;

	FLOAT H_frust = 4.224978f;
	FLOAT W_frust = H_frust * (Sw / Sh);

	const FLOAT fRatioX = 1.0f / (Sw / mcx);
	const FLOAT fRatioY = 1.0f / (Sh / mcy);

	const FLOAT fVirtualCenterX = Sw / 2.0f;
	const FLOAT fVirtualCenterY = Sh / 2.0f;

	g_vMonitorShift[0] = (mx - vx + mcx * 0.5f - fVirtualCenterX) / mcx;
	g_vMonitorShift[1] = -(my - vy + mcy * 0.5f - fVirtualCenterY) / mcy;

	FLOAT fTargetDim = 1.0f;

	const FLOAT fTargetBaseX = 1.0f;
	const FLOAT fTargetBaseY = -0.7f;
	const FLOAT fTargetBaseZ = 3.0f;

	const FLOAT fTargetOffsetX = fRatioX * -g_AeroFlipCfg.sConfig.iHorizontalSpacingMM / 1000.0f;
	const FLOAT fTargetOffsetY = fRatioY * g_AeroFlipCfg.sConfig.iVerticalSpacingMM / 1000.0f;
	const FLOAT fTargetOffsetZ = 1.5f;

	const FLOAT fBaseScale = 1.2f;

	const BOOL bDismissToDesktop = g_bIsDismissing
		&& g_uActiveIndex < (UINT)g_DrawObjects.size()
		&& g_DrawObjects[g_uActiveIndex].bDesktopBg;

	if (g_bIsDismissing)
	{
		fTargetDim = 0.0f;
	}
	else
	{
		fTargetDim = 1.0f;
	}


	for (INT i = 0; i < iNumWindows; ++i)
	{
		auto& window = g_DrawObjects[i];
		INT iRelativeIndex = (i - (INT)g_uActiveIndex + iNumWindows) % iNumWindows;

		FLOAT fTargetX = fTargetBaseX + iRelativeIndex * fTargetOffsetX;
		FLOAT fTargetY = fTargetBaseY + iRelativeIndex * fTargetOffsetY;
		FLOAT fTargetZ = fTargetBaseZ + iRelativeIndex * fTargetOffsetZ;

		//FLOAT fScaleRatio = min(fRatioX, fRatioY);
		FLOAT fTargetScaleX = fRatioY * fBaseScale;
		FLOAT fTargetScaleY = fRatioY * fBaseScale;

		FLOAT fTargetRotY = -30.0f;
		FLOAT fTargetOpacity = 1.0f;

		if (iRelativeIndex >= (INT)uMaxShowWindows)
		{
			fTargetOpacity = max(0.0f, 0.75f - 0.25f * (iRelativeIndex - uMaxShowWindows));
		}

		if (g_bIsDismissing)
		{
			// If windows are minimised, fade out similar way
			if (bDismissToDesktop || IsIconic(window.hTargetWnd) && window.bFocused)
			{
				fTargetX = window.fPosition[0];
				fTargetY = window.fPosition[1];
				fTargetZ = (iRelativeIndex == 0) ? 0.1f : window.fPosition[2];
				fTargetRotY = window.fRotationY;
				fTargetScaleX = window.fScale[0];
				fTargetScaleY = window.fScale[1];
				fTargetOpacity = 0.0f;
			}
			else
			{
				// RESTORE – normal window selected
				// Animate every card back to its actual screen position.
				FLOAT w = (FLOAT)(window.rcBounds.right - window.rcBounds.left);
				if (w <= 0)
				{
					w = 100.0f;
				}
				FLOAT h = (FLOAT)(window.rcBounds.bottom - window.rcBounds.top);
				if (h <= 0)
				{
					h = 100.0f;
				}
				FLOAT cx = window.rcBounds.left + w / 2.0f;
				FLOAT cy = window.rcBounds.top + h / 2.0f;

				fTargetX = (((cx - vx) / Sw) * 2.0f - 1.0f - g_vMonitorShift[0]) * (W_frust / 2.0f);
				fTargetY = (1.0f - ((cy - vy) / Sh) * 2.0f - g_vMonitorShift[1]) * (H_frust / 2.0f);
				fTargetZ = 0.1f;
				fTargetRotY = 0.0f;
				fTargetOpacity = 1.0f;

				FLOAT matchScale = (H_frust / Sh) * (h / 2.0f);
				fTargetScaleX = matchScale;
				fTargetScaleY = matchScale;

				// Fade out the desktop card and, when not showing the live
				// desktop preview, fade out every non-selected card too.
				if (iRelativeIndex != 0 && (window.bDesktopBg || !g_AeroFlipCfg.sConfig.bShowDesktopWhenFlipping))
				{
					fTargetOpacity = 0.0f;
				}
			}
		}
		else if (iRelativeIndex == (iNumWindows - 1) && g_bIsCycling)
		{
			// FLIPPED
			if (window.dwMoveMode != aeroflip::eWDOMM_MOVING_TO_BACK)
			{
				window.dwMoveMode = aeroflip::eWDOMM_MOVING_TO_BACK;
				window.iZOrder = 999;
			}

			if (window.fOpacity > 0.10f)
			{
				// move forward beyond screen
				fTargetOpacity = 0.0f;
				fTargetX = fTargetBaseX - fTargetOffsetX;
				fTargetY = fTargetBaseY - fTargetOffsetY;
				fTargetZ = fTargetBaseZ - fTargetOffsetZ;
			}
			else
			{
				INT backIndex = iNumWindows - 1;
				window.iZOrder = backIndex;
				g_bIsCycling = FALSE;

				// teleport backwards, beyond screen
				if (window.dwMoveMode == aeroflip::eWDOMM_MOVING_TO_BACK)
				{
					// shift slightly
					FLOAT fOffsetIndex = backIndex + 0.5f;
					window.fPosition[0] = fTargetBaseX + fOffsetIndex * fTargetOffsetX;
					window.fPosition[1] = fTargetBaseY + fOffsetIndex * fTargetOffsetY;
					window.fPosition[2] = fTargetBaseZ + fOffsetIndex * fTargetOffsetZ;
					window.dwMoveMode = aeroflip::eWDOMM_DEFAULT;
				}

				// Target in the back now
				fTargetX = fTargetBaseX + backIndex * fTargetOffsetX;
				fTargetY = fTargetBaseY + backIndex * fTargetOffsetY;
				fTargetZ = fTargetBaseZ + backIndex * fTargetOffsetZ;
				fTargetOpacity = max(0.0f, 1.0f - ((backIndex - ((INT)uMaxShowWindows - 2)) * 0.5f));
				fLerpFactor *= 1.5f;
			}
		}
		else
		{
			window.iZOrder = iRelativeIndex;
		}

		window.fPosition[0] += (fTargetX - window.fPosition[0]) * fLerpFactor;
		window.fPosition[1] += (fTargetY - window.fPosition[1]) * fLerpFactor;
		window.fPosition[2] += (fTargetZ - window.fPosition[2]) * fLerpFactor;
		window.fRotationY += (fTargetRotY - window.fRotationY) * fLerpFactor;
		window.fOpacity += (fTargetOpacity - window.fOpacity) * fLerpFactor;

		window.fScale[0] += (fTargetScaleX - window.fScale[0]) * fLerpFactor;
		window.fScale[1] += (fTargetScaleY - window.fScale[1]) * fLerpFactor;
	}
	g_fDesktopDimmingFactor += (fTargetDim - g_fDesktopDimmingFactor) * fLerpFactor;
	g_fDesktopDimmingFactor = max(min(g_fDesktopDimmingFactor, 1.0f), 0.0f);
}

void InitializeWindowEventHook()
{
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
	if (!g_hKeyboardHook)
	{
		OutputDebugString(L"Failed to install Low-Level Keyboard Hook!\n");
	}

	g_hEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_DESTROY,
		NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
}

void CleanupWindowEventHook()
{
	if (g_hKeyboardHook)
	{
		UnhookWindowsHookEx(g_hKeyboardHook);
	}

	if (g_hEventHook)
	{
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
		BOOL bAnyWindowFocused = FALSE;
		for (size_t idx = 0; idx < g_DrawObjects.size(); ++idx)
		{
			if (g_DrawObjects[idx].hTargetWnd == g_hLastActiveWindow)
			{
				bAnyWindowFocused = TRUE;
				break;
			}
		}
		if (bAnyWindowFocused)
		{
			pShellDispatch->ToggleDesktop();
		}
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

//void HideWindowsFromView(HWND hWnd)
//{
//	UINT cWindows = 0;
//	const aeroflip::SWindowTarget* pWindows = NULL;
//	g_pWindowProvider->QueryWindows(&pWindows, &cWindows);
//
//	for (UINT i = 0; i < cWindows; ++i)
//	{
//		if (!pWindows[i].bDesktopWindow)
//		{
//			BOOL bCloak = TRUE;
//			DwmSetWindowAttribute(pWindows[i].hWnd, DWMWA_CLOAK, &bCloak, sizeof(bCloak));
//		}
//	}
//	DwmpActivateLivePreview(TRUE, NULL, hWnd, 3, NULL, 0x3244);
//}
//
//
//
//void RestoreWindowsFromView(HWND hWnd)
//{
//	UINT cWindows = 0;
//	const aeroflip::SWindowTarget* pWindows = NULL;
//	g_pWindowProvider->QueryWindows(&pWindows, &cWindows);
//
//	for (UINT i = 0; i < cWindows; ++i)
//	{
//		if (!pWindows[i].bDesktopWindow)
//		{
//			BOOL bCloak = FALSE;
//			DwmSetWindowAttribute(pWindows[i].hWnd, DWMWA_CLOAK, &bCloak, sizeof(bCloak));
//			DwmpActivateLivePreview(FALSE, pWindows[i].hWnd, hWnd, DWMP_WINDOW, NULL, 0x3244);
//		}
//	}
//	DwmpActivateLivePreview(FALSE, NULL, hWnd, 30, NULL, 0x3244);
//}