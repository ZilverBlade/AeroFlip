#pragma once

#define WIN32_LEAN_AND_MEAN

#include "SWindowTarget.h"
#include <windef.h>

#include <vector>

namespace aeroflip
{
	struct SWindowProviderConfig
	{
		HWND hWnd;
		const WCHAR* szAppWindowClass;
	};
	class CWindowProvider
	{
	public:
		CWindowProvider(const SWindowProviderConfig* pConfig);
		~CWindowProvider();

		void UpdateWindowList();
		void FlagActiveWindow(HWND hWnd);
		HWND HGetActiveWindow();
		void QueryWindows(const SWindowTarget** ppWindowTargets, UINT* pCount);
	private:
		std::vector<SWindowTarget> m_ActiveWindows;

		HWND m_hLastActiveWindow = NULL;
		HWND m_hWindow = NULL;
		const WCHAR* m_szAppWindowClass = NULL;
	};
}
