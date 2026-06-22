#pragma once

#define WIN32_LEAN_AND_MEAN

#include "SWindowTarget.h"
#include <windef.h>

#include <vector>

namespace aeroflip
{
	class CWindowProvider
	{
	public:
		CWindowProvider(HWND hWnd, const WCHAR* szAppWindowClass);
		~CWindowProvider();

		void UpdateWindowList();
		void FlagActiveWindow(HWND hWnd);
		HWND HGetActiveWindow();
		void SetWindowMinimizedFlag(HWND hWnd, BOOL bMinimized);

		void InvalidateWindow(HWND hWnd);
		void MarkWindowUpdated(HWND hWnd);
		void ClearWindowCache(HWND hWnd);
		void InvalidateAllWindows();

		void QueryWindows(const SWindowTarget** ppWindowTargets, UINT* pCount);

		void CacheWindowThumbnail(HWND hWnd);

		BOOL BWindowHasCache(HWND hWnd);

	private:
		void ClearWindowList();

		std::vector<SWindowTarget> m_ActiveWindows;

		HWND m_hLastActiveWindow = NULL;
		HWND m_hWindow = NULL;
		const WCHAR* m_szAppWindowClass = NULL;
	};
}
