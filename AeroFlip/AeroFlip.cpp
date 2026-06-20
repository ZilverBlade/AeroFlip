// AeroFlip.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "AeroFlip.h"
#include "CRendererApi.h" 

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE g_hInst = NULL;						// current instance
TCHAR g_szTitle[MAX_LOADSTRING];				// The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];			// the main window class name
aeroflip::CRendererApi* g_pRenderer = NULL;		// D3D9 renderer

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

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

	aeroflip::SRendererApiConfig config;
	config.hWnd = hWnd;
	config.bHardwareAcceleration = TRUE; // Toggle as needed
	config.uMultiSampleLevel = 0;        // Start at 0 (None) to keep setup simple

	try {
		g_pRenderer = new aeroflip::CRendererApi(&config);
	}
	catch (std::exception ex) {
		OutputDebugStringA(ex.what());
		return FALSE;
	}

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
			g_pRenderer->OnRender();
		}
	}

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
	UNREFERENCED_PARAMETER(hInstance);
	BOOL bResult = TRUE;
	if (nCmdShow) {
		HWND hConsoleWnd = GetConsoleWindow();
		ShowWindow(hConsoleWnd, nCmdShow);
	}
	return bResult;
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