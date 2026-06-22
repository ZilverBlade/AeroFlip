// AeroFlipConfigurator.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include "AeroFlipConfigurator.h"
#include "SAeroFlipConfig.h"

#include <commctrl.h>
#include <tlhelp32.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text

// Global State
aeroflip::SAeroFlipConfig g_AeroFlipCfg;

// Forward declarations of functions included in this code module:
INT_PTR CALLBACK    MasterDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void				LoadConfig();

struct SWndEvent
{
	HWND hDlg;
};

void				OnBtnOk(const SWndEvent* pEvent);
void				OnBtnApply(const SWndEvent* pEvent);
void				OnBtnClose(const SWndEvent* pEvent);
void				OnCbnRendererModeSelect(const SWndEvent* pEvent, int nSelection);
void				OnCbnMsaaQualitySelect(const SWndEvent* pEvent, int nSelection);
void				OnCbnTextureQualitySelect(const SWndEvent* pEvent, int nSelection);
void				OnCbnShortcutSelect(const SWndEvent* pEvent, int nSelection);

void TerminateProcess(LPWSTR lpszProcName)
{
	UNREFERENCED_PARAMETER(lpszProcName);
}

void LaunchProcess(LPWSTR lpszProcPath)
{
	UNREFERENCED_PARAMETER(lpszProcPath);
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	(void)::CreateMutex(NULL,
		TRUE,
		TEXT("Zilverblade_Aero_Flip_Configurator_Mtx"));
	switch (::GetLastError()) {
	case ERROR_SUCCESS:
		// Process was not running already
		break;
	case ERROR_ALREADY_EXISTS:
		OutputDebugString(TEXT("Configurator instance already running\n"));
		return TRUE;
	default:
		OutputDebugString(TEXT("Unknown error occurred creating mutex, terminating...\n"));
		return FALSE;
	}

	LoadConfig();

	InitCommonControls();

	hInst = hInstance;
	MSG msg;

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	HWND hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MASTERFORM), NULL, MasterDlgProc);

	if (!hDlg)
	{
		MessageBox(NULL, _T("Failed to start config!"), _T("Error"), MB_ICONERROR);
		return FALSE;
	}

	ShowWindow(hDlg, nCmdShow);
	UpdateWindow(hDlg);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

INT_PTR CALLBACK MasterDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hDlg, IDC_RENDERER_MODE, CB_ADDSTRING, 0, (LPARAM)_T("D3D9Ex"));

		SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("1x"));
		SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("2x"));
		SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("4x"));
		SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("8x"));
		SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("16x"));

		SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("Nearest"));
		SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("Smooth"));
		SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_ADDSTRING, 0, (LPARAM)_T("Smooth Mip"));

		SendDlgItemMessage(hDlg, IDC_SHORTCUT_MODE, CB_ADDSTRING, 0, (LPARAM)_T("Alt+Tab"));
		SendDlgItemMessage(hDlg, IDC_SHORTCUT_MODE, CB_ADDSTRING, 0, (LPARAM)_T("Win+Tab"));


		SendDlgItemMessage(hDlg, IDC_RENDERER_MODE, CB_SETCURSEL, 0, 0);
		SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_SETCURSEL, 2, 0);
		SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_SETCURSEL, 1, 0);
		SendDlgItemMessage(hDlg, IDC_SHORTCUT_MODE, CB_SETCURSEL, 0, 0);

		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		int wmEvent = HIWORD(wParam); // Used to detect control actions

		SWndEvent event;
		ZeroMemory(&event, sizeof(SWndEvent));
		event.hDlg = hDlg;

		if (wmEvent == CBN_SELCHANGE)
		{
			int sel = (int)SendDlgItemMessage(hDlg, wmId, CB_GETCURSEL, 0, 0);
			switch (wmId)
			{
			case IDC_RENDERER_MODE:
				OnCbnRendererModeSelect(&event, sel);
				return (INT_PTR)TRUE;
			case IDC_MSAA_QUALITY:
				OnCbnMsaaQualitySelect(&event, sel);
				return (INT_PTR)TRUE;
			case IDC_TEXTURE_QUALITY:
				OnCbnTextureQualitySelect(&event, sel);
				return (INT_PTR)TRUE;
			case IDC_SHORTCUT_MODE:
				OnCbnShortcutSelect(&event, sel);
				return (INT_PTR)TRUE;
			}
		}

		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hDlg, About);
			return (INT_PTR)TRUE;
		case IDM_START:
			LaunchProcess(L"C:/Program Files/AeroFlip.exe");
			return (INT_PTR)TRUE;
		case IDM_EXIT:
			TerminateProcess(L"AeroFlip.exe");
			return (INT_PTR)TRUE;

		case IDC_APPLY:
			OnBtnApply(&event);
			return (INT_PTR)TRUE;
		case IDC_OK:
			OnBtnOk(&event);
			return (INT_PTR)TRUE;
		case IDC_CLOSE:
			OnBtnClose(&event);
			return (INT_PTR)TRUE;
		}
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void LoadConfig()
{
	g_AeroFlipCfg.kbConfig.InitDefault();
	g_AeroFlipCfg.rConfig.InitDefault();
	g_AeroFlipCfg.sConfig.InitDefault();
}


void OnBtnOk(const SWndEvent* pEvent)
{
	OnBtnApply(pEvent);
	OnBtnClose(pEvent);
}
void OnBtnApply(const SWndEvent* pEvent)
{
	UNREFERENCED_PARAMETER(pEvent);
}
void OnBtnClose(const SWndEvent* pEvent)
{
	DestroyWindow(pEvent->hDlg);
}

void OnCbnRendererModeSelect(const SWndEvent* pEvent, int nSelection)
{
	UNREFERENCED_PARAMETER(pEvent);
	switch (nSelection)
	{
	case 0:
		g_AeroFlipCfg.rConfig.dwRendererMode = aeroflip::eRM_D3D9_EX;
		break;
	}
}

void OnCbnMsaaQualitySelect(const SWndEvent* pEvent, int nSelection)
{
	UNREFERENCED_PARAMETER(pEvent);
	switch (nSelection)
	{
	case 0:
		g_AeroFlipCfg.rConfig.uMultiSampleLevel = 1;
		break;
	case 1:
		g_AeroFlipCfg.rConfig.uMultiSampleLevel = 2;
		break;
	case 2:
		g_AeroFlipCfg.rConfig.uMultiSampleLevel = 4;
		break;
	case 3:
		g_AeroFlipCfg.rConfig.uMultiSampleLevel = 8;
		break;
	case 4:
		g_AeroFlipCfg.rConfig.uMultiSampleLevel = 16;
		break;
	}
}

void OnCbnTextureQualitySelect(const SWndEvent* pEvent, int nSelection)
{
	UNREFERENCED_PARAMETER(pEvent);
	switch (nSelection)
	{
	case 0:
		g_AeroFlipCfg.rConfig.dwTextureQuality = aeroflip::eTQ_NEAREST;
		break;
	case 1:
		g_AeroFlipCfg.rConfig.dwTextureQuality = aeroflip::eTQ_SMOOTH;
		break;
	case 2:
		g_AeroFlipCfg.rConfig.dwTextureQuality = aeroflip::eTQ_SMOOTH_MIP;
		break;
	}
}

void OnCbnShortcutSelect(const SWndEvent* pEvent, int nSelection)
{
	UNREFERENCED_PARAMETER(pEvent);
	switch (nSelection)
	{
	case 0:
		g_AeroFlipCfg.kbConfig.dwFlipShortcutMode = aeroflip::eFSM_ALT_TAB;
		break;
	case 1:
		g_AeroFlipCfg.kbConfig.dwFlipShortcutMode = aeroflip::eFSM_WIN_TAB;
		break;
	}
}