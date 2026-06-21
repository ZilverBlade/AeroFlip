#pragma once

#include <windef.h>

namespace aeroflip 
{
	struct SWindowTarget 
	{
		BOOL bDesktopWindow;
		BOOL bDecorated;
		BOOL bActive;
		HWND hWnd;
		UINT uWindowZOrder;
		WCHAR szTitle[256];

		BOOL bMinimized;
		UINT uCachedWidth;
		UINT uCachedHeight;
		BYTE* pCachedPixels;
		UINT cbCachedPixels;

		RECT rcCachedBounds;
		BOOL bHasCachedBounds;

		BOOL bNeedsUpdate;
	};
}