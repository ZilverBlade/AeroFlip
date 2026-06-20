#pragma once

#define WIN32_LEAN_AND_MEAN

#include "SWindowTarget.h"
#include <Windows.h>

#include <vector>

namespace aeroflip{
	struct SWindowProviderConfig {
		HWND hWnd;
		const WCHAR* szAppWindowClass;
	};
	class CWindowProvider {
	public:
		CWindowProvider(const SWindowProviderConfig* pConfig);
		~CWindowProvider();

		void UpdateWindowList();
		HWND HGetFocusedWindow();
		HWND HEnumWindows(UINT* pIndex);
	private:
		std::vector<SWindowTarget> m_ActiveWindows;

		HWND m_hWindow = NULL;
		const WCHAR* m_szAppWindowClass = NULL;
	};
}
