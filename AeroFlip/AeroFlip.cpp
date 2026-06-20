// AeroFlip.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "AeroFlip.h"
#include "CRendererApi.h" 
#include "CWindowProvider.h" 

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE g_hst = NULL;								// current instance
TCHAR g_szTitle[MAX_LOADSTRING];						// The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];					// the main window class name
HWINEVENTHOOK g_hEventHook = NULL;
aeroflip::CRendererApi* g_pRenderer = NULL;				// D3D9 renderer
aeroflip::CWindowProvider* g_pWindowProvider = NULL;	// DWMAPI Provider
BOOL g_bWindowDirty = TRUE;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK		WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
void				MakeWindowTransparent(HWND);
void				InitializeWindowEventHook();
void				CleanupWindowEventHook();

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;

	LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_AEROFLIP, g_szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AEROFLIP));

	HWND hWnd = FindWindow(g_szWindowClass, g_szTitle);

	try {
		aeroflip::SRendererApiConfig config;
		ZeroMemory(&config, sizeof(aeroflip::SRendererApiConfig));
		config.hWnd = hWnd;
		config.bHardwareAcceleration = TRUE;
		config.uMultiSampleLevel = 0;

		g_pRenderer = new aeroflip::CRendererApi(&config);
	}
	catch (std::exception ex) {
		OutputDebugStringA(ex.what());
		OutputDebugStringA("\n");
		return FALSE;
	}

	try {
		aeroflip::SWindowProviderConfig config;
		ZeroMemory(&config, sizeof(aeroflip::SWindowProviderConfig));
		config.hWnd = hWnd;
		config.szAppWindowClass = g_szWindowClass;

		g_pWindowProvider = new aeroflip::CWindowProvider(&config);
	}
	catch (std::exception ex) {
		delete g_pRenderer;
		g_pRenderer = NULL;
		OutputDebugStringA(ex.what());
		OutputDebugStringA("\n");
		return FALSE;
	}

	InitializeWindowEventHook();

	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			try {
				if (g_bWindowDirty) {
					g_pWindowProvider->UpdateWindowList();
					g_pRenderer->UpdateWindows(g_pWindowProvider);
					g_bWindowDirty = FALSE;
				}
				g_pRenderer->OnRender(0.0f);
			}
			catch (std::exception ex) {
				OutputDebugStringA(ex.what());
				OutputDebugStringA("\n");
				msg.message = WM_QUIT;
				msg.wParam = 1;
			}
		}
	}

	CleanupWindowEventHook();

	delete g_pRenderer;
	g_pRenderer = NULL;
	delete g_pWindowProvider;
	g_pWindowProvider = NULL;
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

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
	g_hst = hInstance;

	HWND hWnd = CreateWindow(g_szWindowClass, g_szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	MakeWindowTransparent(hWnd);
	UpdateWindow(hWnd);

	return TRUE;
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

void MakeWindowTransparent(HWND hWnd) {
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
	DWORD dwmsEventTime) {

	UNREFERENCED_PARAMETER(hWinEventHook);
	UNREFERENCED_PARAMETER(hWnd);
	UNREFERENCED_PARAMETER(dwEventThread);
	UNREFERENCED_PARAMETER(dwmsEventTime);

	if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

	switch (event) {
	case EVENT_OBJECT_DESTROY: {
		OutputDebugString(L"A window was closed. Refreshing deck list.\n");
		g_bWindowDirty = TRUE;
		break;
	}

	case EVENT_SYSTEM_MOVESIZEEND: {
		OutputDebugString(L"A window was resized/moved. Updating target texture.\n");
		g_bWindowDirty = TRUE;
		break;
	}

	case EVENT_SYSTEM_MINIMIZESTART: {
		break;
	}

	case EVENT_SYSTEM_MINIMIZEEND: {
		OutputDebugString(L"A window was restored. Updating target texture.\n");
		g_bWindowDirty = TRUE;
		break;
	}
	}
}
void InitializeWindowEventHook() {
	g_hEventHook = SetWinEventHook(
		EVENT_SYSTEM_MOVESIZESTART,
		EVENT_OBJECT_DESTROY,
		NULL,
		WinEventProc,
		0,
		0,
		WINEVENT_OUTOFCONTEXT);
}

void CleanupWindowEventHook() {
	if (g_hEventHook) {
		UnhookWinEvent(g_hEventHook);
	}
}