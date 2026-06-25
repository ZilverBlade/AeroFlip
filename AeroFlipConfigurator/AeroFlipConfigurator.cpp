// AeroFlipConfigurator.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include "AeroFlipConfigurator.h"
#include "SAeroFlipConfig.h"
#include "CommonsCfg.h"

#include <commctrl.h>
#include <shellapi.h>
#include <process.h>
#include <Tlhelp32.h>
#include <winbase.h>

#include <string>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE g_hInst;                                // current instance
TCHAR g_szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR g_szInstallPageUrl[MAX_LOADSTRING];         // The install page iurl

// Global State
aeroflip::SAeroFlipConfig g_AeroFlipCfg;
BOOL g_bDirtyChanges = FALSE;

// Forward declarations of functions included in this code module:
INT_PTR CALLBACK    MasterDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void				LoadConfig();
void				StoreConfig();

void				SynchronizeDialog(HWND hWnd);

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
void				OnCbnWindFrameStyleSelect(const SWndEvent* pEvent, int nSelection);
void				OnCbnAnimSpeedSelect(const SWndEvent* pEvent, int nSelection);

DWORD GetProcessIdByName(LPCTSTR lpszProcessName)
{
	DWORD processId = 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (Process32FirstW(hSnapshot, &pe32))
		{
			do
			{
				if (_tcsicmp(pe32.szExeFile, lpszProcessName) == 0)
				{
					processId = pe32.th32ProcessID;
					break;
				}
			} while (Process32NextW(hSnapshot, &pe32));
		}

		CloseHandle(hSnapshot);
	}

	return processId;
}

HWND GetWindowHandleByProcessId(DWORD processId)
{
	if (processId == 0) return NULL;


	struct FindWindowData {
		DWORD dwProcessId;
		HWND hWndFound;

		static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
		{
			FindWindowData* pData = (FindWindowData*)lParam;
			DWORD dwWindowPid = 0;

			GetWindowThreadProcessId(hWnd, &dwWindowPid);

			if (dwWindowPid == pData->dwProcessId)
			{
				pData->hWndFound = hWnd;
				return FALSE;
			}

			return TRUE;
		}
	};


	FindWindowData data = { 0 };
	data.dwProcessId = processId;
	data.hWndFound = NULL;

	EnumWindows(FindWindowData::EnumWindowsProc, (LPARAM)&data);

	return data.hWndFound;
}

// https://stackoverflow.com/a/7956651
void KillProcessByName(LPCTSTR filename)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
	PROCESSENTRY32 pEntry;
	pEntry.dwSize = sizeof(pEntry);
	BOOL hRes = Process32First(hSnapShot, &pEntry);
	while (hRes)
	{
		if (lstrcmp(pEntry.szExeFile, filename) == 0)
		{
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0,
				(DWORD)pEntry.th32ProcessID);
			if (hProcess != NULL)
			{
				TerminateProcess(hProcess, 9);
				CloseHandle(hProcess);
			}
		}
		hRes = Process32Next(hSnapShot, &pEntry);
	}
	CloseHandle(hSnapShot);
}

void TerminateAeroFlipProcess()
{
	KillProcessByName(TEXT("AeroFlip.exe"));
}

void LaunchAeroFlipProcess()
{
	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);

	TCHAR* lastSlash = _tcsrchr(exePath, L'\\');
	if (lastSlash != NULL)
	{
		*(lastSlash + 1) = TEXT('\0');
	}

	lstrcat(exePath, TEXT("AeroFlip.exe"));

	HINSTANCE hInst = ShellExecute(
		NULL,
		L"open",
		exePath,
		NULL,
		exePath,
		SW_SHOWNORMAL
		);

	if ((INT_PTR)hInst <= 32)
	{
		OutputDebugString(TEXT("Failed to launch AeroFlip.exe"));
	}
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
	{
		OutputDebugString(TEXT("Configurator instance already running, focussing...\n"));
		HWND hWnd = GetWindowHandleByProcessId(GetProcessIdByName(TEXT("AeroFlipConfigurator.exe")));
		SetForegroundWindow(hWnd);
		return TRUE;
	}
	default:
		OutputDebugString(TEXT("Unknown error occurred creating mutex, terminating...\n"));
		return FALSE;
	}

	LoadConfig();

	InitCommonControls();

	g_hInst = hInstance;
	MSG msg;

	LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDS_INSTALL_URL, g_szInstallPageUrl, MAX_LOADSTRING);

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
		//SendDlgItemMessage(hDlg, IDC_RENDERER_MODE, CB_ADDSTRING, 0, (LPARAM)_T("D3D11 (WGC)"));

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

		SendDlgItemMessage(hDlg, IDC_WINDOW_FRAME_STYLE, CB_ADDSTRING, 0, (LPARAM)_T("None"));
		SendDlgItemMessage(hDlg, IDC_WINDOW_FRAME_STYLE, CB_ADDSTRING, 0, (LPARAM)_T("<NOT IMPLEMENTED>"));
		SendDlgItemMessage(hDlg, IDC_WINDOW_FRAME_STYLE, CB_ADDSTRING, 0, (LPARAM)_T("Windows Aero"));

		SendDlgItemMessage(hDlg, IDC_ANIM_SPEED, CB_ADDSTRING, 0, (LPARAM)_T("Slow"));
		SendDlgItemMessage(hDlg, IDC_ANIM_SPEED, CB_ADDSTRING, 0, (LPARAM)_T("Normal"));
		SendDlgItemMessage(hDlg, IDC_ANIM_SPEED, CB_ADDSTRING, 0, (LPARAM)_T("Fast"));

		SendDlgItemMessage(hDlg, IDC_DESKTOP_DIMMING_PERC, EM_SETLIMITTEXT, 3, 0);
		SendDlgItemMessage(hDlg, IDC_MAX_WINDOWS_VISIBLE, EM_SETLIMITTEXT, 1, 0);
		SendDlgItemMessage(hDlg, IDC_HORIZONTAL_SPACING, EM_SETLIMITTEXT, 4, 0);
		SendDlgItemMessage(hDlg, IDC_VERTICAL_SPACING, EM_SETLIMITTEXT, 4, 0);

		SynchronizeDialog(hDlg);

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
			case IDC_WINDOW_FRAME_STYLE:
				OnCbnWindFrameStyleSelect(&event, sel);
				return (INT_PTR)TRUE;
			case IDC_ANIM_SPEED:
				OnCbnAnimSpeedSelect(&event, sel);
				return (INT_PTR)TRUE;
			}
		}

		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hDlg, About);
			return (INT_PTR)TRUE;
		case IDM_START:
			LaunchAeroFlipProcess();
			return (INT_PTR)TRUE;
		case IDM_EXIT:
			TerminateAeroFlipProcess();
			return (INT_PTR)TRUE;
		case IDM_INSTALL_PAGE:
			ShellExecute(NULL, TEXT("open"), g_szInstallPageUrl, NULL, NULL, SW_SHOWNORMAL);
			break;
		case IDC_APPLY:
			OnBtnApply(&event);
			return (INT_PTR)TRUE;
		case IDC_OK:
			OnBtnOk(&event);
			return (INT_PTR)TRUE;
		case IDC_CLOSE:
			OnBtnClose(&event);
			return (INT_PTR)TRUE;

		default: // Rest of control inputs
			g_bDirtyChanges = TRUE;
			EnableWindow(GetDlgItem(hDlg, IDC_APPLY), TRUE);
			break;
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
	// set defaults first
	g_AeroFlipCfg.kbConfig.InitDefault();
	g_AeroFlipCfg.rConfig.InitDefault();
	g_AeroFlipCfg.sConfig.InitDefault();

	DefineIni(IniRead, &g_AeroFlipCfg);
}

void StoreConfig()
{
	DefineIni(IniWrite, &g_AeroFlipCfg);
}

void SynchronizeDialog(HWND hDlg)
{
	// KeyBind
	{
		SendDlgItemMessage(hDlg, IDC_CYCLE_ON_FIRST_TAB, BM_SETCHECK, g_AeroFlipCfg.kbConfig.bCycleOnFirstTab ? 1 : 0, 0);
		SendDlgItemMessage(hDlg, IDC_SHIFT_TO_MOVE_BACK, BM_SETCHECK, g_AeroFlipCfg.kbConfig.bShiftToMoveBack ? 1 : 0, 0);

		switch (g_AeroFlipCfg.kbConfig.dwFlipShortcutMode)
		{
		case aeroflip::eFSM_WIN_TAB:
			SendDlgItemMessage(hDlg, IDC_SHORTCUT_MODE, CB_SETCURSEL, 1, 0);
			break;
		case aeroflip::eFSM_ALT_TAB:
			SendDlgItemMessage(hDlg, IDC_SHORTCUT_MODE, CB_SETCURSEL, 0, 0);
			break;
		}
	}

	// Style
	{
		SendDlgItemMessage(hDlg, IDC_SHOW_DESKTOP, BM_SETCHECK, g_AeroFlipCfg.sConfig.bShowDesktopWhenFlipping ? 1 : 0, 0);

		SendDlgItemMessage(hDlg, IDC_DESKTOP_DIMMING_PERC, EM_REPLACESEL, 0,
			(LPARAM)(std::to_wstring(g_AeroFlipCfg.sConfig.uDesktopDimmingPercent).c_str()));
		SendDlgItemMessage(hDlg, IDC_MAX_WINDOWS_VISIBLE, EM_REPLACESEL, 0,
			(LPARAM)(std::to_wstring(g_AeroFlipCfg.sConfig.uMaxWindowsVisible).c_str()));
		SendDlgItemMessage(hDlg, IDC_HORIZONTAL_SPACING, EM_REPLACESEL, 0,
			(LPARAM)(std::to_wstring(g_AeroFlipCfg.sConfig.iHorizontalSpacingMM).c_str()));
		SendDlgItemMessage(hDlg, IDC_VERTICAL_SPACING, EM_REPLACESEL, 0,
			(LPARAM)(std::to_wstring(g_AeroFlipCfg.sConfig.iVerticalSpacingMM).c_str()));

		switch (g_AeroFlipCfg.sConfig.dwWindowFrameStyle)
		{
		case aeroflip::eWFS_NONE:
			SendDlgItemMessage(hDlg, IDC_WINDOW_FRAME_STYLE, CB_SETCURSEL, 0, 0);
			break;
		case aeroflip::eWFS_NATIVE:
			SendDlgItemMessage(hDlg, IDC_WINDOW_FRAME_STYLE, CB_SETCURSEL, 1, 0);
			break;
		case aeroflip::eWFS_WINDOWS_AERO:
			SendDlgItemMessage(hDlg, IDC_WINDOW_FRAME_STYLE, CB_SETCURSEL, 2, 0);
			break;
		}

		switch (g_AeroFlipCfg.sConfig.iAnimationSpeed)
		{
		case kAS_SLOW:
			SendDlgItemMessage(hDlg, IDC_ANIM_SPEED, CB_SETCURSEL, 0, 0);
			break;
		case kAS_NORMAL:
			SendDlgItemMessage(hDlg, IDC_ANIM_SPEED, CB_SETCURSEL, 1, 0);
			break;
		case kAS_FAST:
			SendDlgItemMessage(hDlg, IDC_ANIM_SPEED, CB_SETCURSEL, 2, 0);
			break;
		}
	}

	// Renderer
	{
		SendDlgItemMessage(hDlg, IDC_HARDWARE_ACCELERATION, BM_SETCHECK, g_AeroFlipCfg.rConfig.bHardwareAcceleration ? 1 : 0, 0);
		SendDlgItemMessage(hDlg, IDC_LIVE_CAPTURE, BM_SETCHECK, g_AeroFlipCfg.rConfig.bLiveCapture ? 1 : 0, 0);
		SendDlgItemMessage(hDlg, IDC_PERSIST_IN_VRAM, BM_SETCHECK, g_AeroFlipCfg.rConfig.bPersistInVRAM ? 1 : 0, 0);
		SendDlgItemMessage(hDlg, IDC_VSYNC, BM_SETCHECK, g_AeroFlipCfg.rConfig.bVSync ? 1 : 0, 0);

		switch (g_AeroFlipCfg.rConfig.uMultiSampleLevel)
		{
		case 1:
			SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_SETCURSEL, 0, 0);
			break;
		case 2:
			SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_SETCURSEL, 1, 0);
			break;
		case 4:
			SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_SETCURSEL, 2, 0);
			break;
		case 8:
			SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_SETCURSEL, 3, 0);
			break;
		case 16:
			SendDlgItemMessage(hDlg, IDC_MSAA_QUALITY, CB_SETCURSEL, 4, 0);
			break;
		}

		switch (g_AeroFlipCfg.rConfig.dwRendererMode)
		{
		case aeroflip::eRM_D3D9_EX:
			SendDlgItemMessage(hDlg, IDC_RENDERER_MODE, CB_SETCURSEL, 0, 0);
			break;
			//case aeroflip::eRM_D3D11:
			//	SendDlgItemMessage(hDlg, IDC_RENDERER_MODE, CB_SETCURSEL, 1, 0);
			//	break;
		}

		switch (g_AeroFlipCfg.rConfig.dwTextureQuality)
		{
		case aeroflip::eTQ_NEAREST:
			SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_SETCURSEL, 0, 0);
			break;
		case aeroflip::eTQ_SMOOTH:
			SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_SETCURSEL, 1, 0);
			break;
		case aeroflip::eTQ_SMOOTH_MIP:
			SendDlgItemMessage(hDlg, IDC_TEXTURE_QUALITY, CB_SETCURSEL, 2, 0);
			break;
		}
	}
}

void OnBtnOk(const SWndEvent* pEvent)
{
	OnBtnApply(pEvent);
	OnBtnClose(pEvent);
}
void OnBtnApply(const SWndEvent* pEvent)
{
	if (g_bDirtyChanges)
	{
		g_AeroFlipCfg.kbConfig.bCycleOnFirstTab =
			(SendDlgItemMessage(pEvent->hDlg, IDC_CYCLE_ON_FIRST_TAB, BM_GETCHECK, 0, 0) == BST_CHECKED);

		g_AeroFlipCfg.kbConfig.bShiftToMoveBack =
			(SendDlgItemMessage(pEvent->hDlg, IDC_SHIFT_TO_MOVE_BACK, BM_GETCHECK, 0, 0) == BST_CHECKED);

		g_AeroFlipCfg.kbConfig.bPressKeyAgainToExit =
			(SendDlgItemMessage(pEvent->hDlg, IDC_PRESS_AGAIN_TO_EXIT, BM_GETCHECK, 0, 0) == BST_CHECKED);


		g_AeroFlipCfg.sConfig.bShowDesktopWhenFlipping =
			(SendDlgItemMessage(pEvent->hDlg, IDC_SHOW_DESKTOP, BM_GETCHECK, 0, 0) == BST_CHECKED);

		{
			TCHAR buf[8];
			ZeroMemory(&buf, sizeof(buf));
			*(WORD*)buf = 6;

			SendDlgItemMessage(pEvent->hDlg, IDC_MAX_WINDOWS_VISIBLE, EM_GETLINE, 0, (LPARAM)buf);
			g_AeroFlipCfg.sConfig.uMaxWindowsVisible = max((UINT)_tstoi(buf), 1); // at least 1 window!
		}
		{
			TCHAR buf[8];
			ZeroMemory(&buf, sizeof(buf));
			*(WORD*)buf = 6;

			SendDlgItemMessage(pEvent->hDlg, IDC_DESKTOP_DIMMING_PERC, EM_GETLINE, 0, (LPARAM)buf);
			g_AeroFlipCfg.sConfig.uDesktopDimmingPercent = max((UINT)_tstoi(buf), 1); // at least 1 window!
		}
		{
			TCHAR buf[8];
			ZeroMemory(&buf, sizeof(buf));
			*(WORD*)buf = 6;

			SendDlgItemMessage(pEvent->hDlg, IDC_HORIZONTAL_SPACING, EM_GETLINE, 0, (LPARAM)buf);
			g_AeroFlipCfg.sConfig.iHorizontalSpacingMM = _tstoi(buf);
		}
		{
			TCHAR buf[8];
			ZeroMemory(&buf, sizeof(buf));
			*(WORD*)buf = 6;

			SendDlgItemMessage(pEvent->hDlg, IDC_VERTICAL_SPACING, EM_GETLINE, 0, (LPARAM)buf);
			g_AeroFlipCfg.sConfig.iVerticalSpacingMM = _tstoi(buf);
		}

		g_AeroFlipCfg.rConfig.bHardwareAcceleration =
			(SendDlgItemMessage(pEvent->hDlg, IDC_HARDWARE_ACCELERATION, BM_GETCHECK, 0, 0) == BST_CHECKED);

		g_AeroFlipCfg.rConfig.bLiveCapture =
			(SendDlgItemMessage(pEvent->hDlg, IDC_LIVE_CAPTURE, BM_GETCHECK, 0, 0) == BST_CHECKED);

		g_AeroFlipCfg.rConfig.bPersistInVRAM =
			(SendDlgItemMessage(pEvent->hDlg, IDC_PERSIST_IN_VRAM, BM_GETCHECK, 0, 0) == BST_CHECKED);

		g_AeroFlipCfg.rConfig.bVSync =
			(SendDlgItemMessage(pEvent->hDlg, IDC_VSYNC, BM_GETCHECK, 0, 0) == BST_CHECKED);

		StoreConfig();
	}
	EnableWindow(GetDlgItem(pEvent->hDlg, IDC_APPLY), FALSE);
	g_bDirtyChanges = FALSE;
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
		//case 1:
		//	g_AeroFlipCfg.rConfig.dwRendererMode = aeroflip::eRM_D3D11;
		//	break;
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

void OnCbnWindFrameStyleSelect(const SWndEvent* pEvent, int nSelection)
{
	UNREFERENCED_PARAMETER(pEvent);
	switch (nSelection)
	{
	case 0:
		g_AeroFlipCfg.sConfig.dwWindowFrameStyle = aeroflip::eWFS_NONE;
		break;
	case 1:
		g_AeroFlipCfg.sConfig.dwWindowFrameStyle = aeroflip::eWFS_NATIVE;
		break;
	case 2:
		g_AeroFlipCfg.sConfig.dwWindowFrameStyle = aeroflip::eWFS_WINDOWS_AERO;
		break;
	}
}

void OnCbnAnimSpeedSelect(const SWndEvent* pEvent, int nSelection)
{
	UNREFERENCED_PARAMETER(pEvent);
	switch (nSelection)
	{
	case 0:
		g_AeroFlipCfg.sConfig.iAnimationSpeed = kAS_SLOW;
		break;
	case 1:
		g_AeroFlipCfg.sConfig.iAnimationSpeed = kAS_NORMAL;
		break;
	case 2:
		g_AeroFlipCfg.sConfig.iAnimationSpeed = kAS_FAST;
		break;
	}
}