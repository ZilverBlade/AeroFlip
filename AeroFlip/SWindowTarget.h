#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace aeroflip 
{
	struct SWindowTarget 
	{
		BOOL bDesktopWindow;
		BOOL bActive;
		HWND hWnd;
		UINT uWindowZOrder;
		WCHAR szTitle[256];

		BOOL bMinimized;
		UINT uCachedWidth;
		UINT uCachedHeight;
		BYTE* pCachedPixels;
		UINT cbCachedPixels;

		BOOL bNeedsUpdate;
	};
}