// AeroFlip.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "AeroFlip.h"
#include "CD3D9ExRendererApi.h" 
#include "CWindowProvider.h" 

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE g_hst = NULL;									// current instance
TCHAR g_szTitle[MAX_LOADSTRING];						// The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];					// the main window class name
HWINEVENTHOOK g_hEventHook = NULL;
HHOOK g_hKeyboardHook = NULL;
aeroflip::IRendererApi* g_pRenderer = NULL;				// Windowing renderer
aeroflip::CWindowProvider* g_pWindowProvider = NULL;	// DWMAPI Provider
BOOL g_bWindowDirty = TRUE;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE);
BOOL				InitInstance(HINSTANCE, int);
void				WakeAeroFlip(HWND);
void				DismissAeroFlip(HWND, HWND);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK		WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
LRESULT CALLBACK	LowLevelKeyboardProc(int, WPARAM, LPARAM);

void				MakeWindowTransparent(HWND);
void				InitializeWindowEventHook();
void				CleanupWindowEventHook();

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
	}

	InitializeWindowEventHook();

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
					DismissAeroFlip(hWnd, g_pWindowProvider->HGetActiveWindow());
					GetAsyncKeyState(VK_MENU);
					continue;
				}

				if (g_bWindowDirty)
				{
					g_pWindowProvider->UpdateWindowList();
					if (g_pRenderer) {
						g_pRenderer->UpdateWindows(g_pWindowProvider);
					}
					g_bWindowDirty = FALSE;
				}
				if (g_pRenderer) {
					g_pRenderer->OnRender(0.0f);
				}
			}
			else
			{
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
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
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
	if (g_pRenderer == NULL)
	{
		aeroflip::SD3D9ExRendererApiConfig rConfig;
		ZeroMemory(&rConfig, sizeof(aeroflip::SD3D9ExRendererApiConfig));

		rConfig.hWnd = hWnd;
		rConfig.bHardwareAcceleration = TRUE;
		rConfig.uMultiSampleLevel = 0;

		g_pRenderer = new aeroflip::CD3D9ExRendererApi(&rConfig);
	}
	g_bWindowDirty = TRUE; // Force an immediate refresh of the window targets
	ShowWindow(hWnd, SW_SHOW);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetForegroundWindow(hWnd);
}

void DismissAeroFlip(HWND hWnd, HWND hSelectedApp)
{
	ShowWindow(hWnd, SW_HIDE);
	// reduce memory usage when dismissed
	if (g_pRenderer != NULL) 
	{
		delete g_pRenderer;
		g_pRenderer = NULL;
	}
	// clear up RAM
	SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
	if (hSelectedApp != NULL)
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
	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hWnd, RGB(0, 255, 255), 0, LWA_COLORKEY);
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
	UNREFERENCED_PARAMETER(hWnd);
	UNREFERENCED_PARAMETER(dwEventThread);
	UNREFERENCED_PARAMETER(dwmsEventTime);

	if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

	switch (event) {
	case EVENT_OBJECT_DESTROY:
	{
		OutputDebugString(L"A window was closed. Refreshing deck list.\n");
		g_bWindowDirty = TRUE;
		break;
	}

	case EVENT_SYSTEM_MOVESIZEEND:
	{
		OutputDebugString(L"A window was resized/moved. Updating target texture.\n");
		g_bWindowDirty = TRUE;
		break;
	}

	case EVENT_SYSTEM_MINIMIZESTART:
	{
		break;
	}

	case EVENT_SYSTEM_MINIMIZEEND:
	{
		OutputDebugString(L"A window was restored. Updating target texture.\n");
		g_bWindowDirty = TRUE;
		break;
	}
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
						g_pWindowProvider->UpdateWindowList();

						HWND hLastActiveWindow = GetForegroundWindow();
						if (hLastActiveWindow != hWnd)
						{
							g_pWindowProvider->FlagActiveWindow(hLastActiveWindow);
						}

						WakeAeroFlip(hWnd);
					}
					else
					{
						if (g_pWindowProvider)
						{
							g_bWindowDirty = TRUE;
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

void InitializeWindowEventHook()
{
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
	if (!g_hKeyboardHook) {
		OutputDebugString(L"Failed to install Low-Level Keyboard Hook!\n");
	}

	g_hEventHook = SetWinEventHook(
		EVENT_SYSTEM_MOVESIZESTART, EVENT_OBJECT_DESTROY,
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