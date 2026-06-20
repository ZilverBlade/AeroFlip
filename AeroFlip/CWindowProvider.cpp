#include "stdafx.h"
#include "CWindowProvider.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <vector>

// Windows 8+ only
#define DWMWA_CLOAKED 13

namespace aeroflip {
	BOOL IsWindows8OrGreater() {
		OSVERSIONINFOEXW osvi = { 0 };
		osvi.dwOSVersionInfoSize = sizeof(osvi);
		osvi.dwMajorVersion = 6;
		osvi.dwMinorVersion = 2; // 6.2 is Windows 8

		DWORDLONG dwlConditionMask = 0;
		VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
		VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

		return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask) != FALSE;
	}

	struct SEnumContext {
		std::vector<SWindowTarget>* pList;
		const WCHAR* szIgnoreClass; 
	};

	// Callback filter logic
	BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
		const SEnumContext* pContext = (const SEnumContext*)lParam;

		// Desktop window is special
		if (hWnd == GetShellWindow()) {
			SWindowTarget target;
			ZeroMemory(&target, sizeof(SWindowTarget));
			target.bDesktopWindow = TRUE;
			target.hWnd = hWnd;
			wcscpy_s(target.szTitle, L"Desktop");
			pContext->pList->push_back(target);
			return TRUE; 
		}

		if (!IsWindowVisible(hWnd)) return TRUE;

		if (IsWindows8OrGreater()) {
			int cloaked = 0;
			DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
			if (cloaked) return TRUE;
		}
		LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_TOOLWINDOW) {
			return TRUE;
		}

		HWND hOwner = GetWindow(hWnd, GW_OWNER);
		if (hOwner != NULL) return TRUE;

		WCHAR szTitle[256];
		ZeroMemory(szTitle, sizeof(szTitle));
		GetWindowText(hWnd, szTitle, 256);
		if (wcslen(szTitle) == 0) return TRUE;

		WCHAR szClassName[256];
		ZeroMemory(szClassName, sizeof(szClassName));
		GetClassName(hWnd, szClassName, 256);
		if (wcsncmp(szClassName, pContext->szIgnoreClass, 256) == 0) return TRUE;

		if (wcscmp(szClassName, L"Windows.UI.Core.CoreWindow") == 0 ||
			wcscmp(szClassName, L"Shell_TrayWnd") == 0 ||
			wcscmp(szClassName, L"Progman") == 0) {
			return TRUE;
		}

		SWindowTarget target;
		ZeroMemory(&target, sizeof(SWindowTarget));
		target.hWnd = hWnd;
		wcscpy_s(target.szTitle, szTitle);
		pContext->pList->push_back(target);

		return TRUE;
	}

	CWindowProvider::CWindowProvider(const SWindowProviderConfig* pConfig)
		: m_hWindow(pConfig->hWnd), m_szAppWindowClass(pConfig->szAppWindowClass) {
		
	}

	CWindowProvider::~CWindowProvider() {
		
	}

	void CWindowProvider::UpdateWindowList() {
		m_ActiveWindows.clear();

		SEnumContext context;
		context.pList = &m_ActiveWindows;
		context.szIgnoreClass = m_szAppWindowClass;
		EnumWindows(EnumWindowsProc, (LPARAM)&context);
	}

	HWND CWindowProvider::HGetFocusedWindow() {
		if (m_ActiveWindows.empty()) return NULL;
		return m_ActiveWindows[0].hWnd;
	}

	HWND CWindowProvider::HEnumWindows(UINT* pIndex) {
		UINT index = (*pIndex)++;

		if (index < (UINT)m_ActiveWindows.size()){
			return m_ActiveWindows[index].hWnd;
		}
		else {
			return NULL;
		}
	}
}
