#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace aeroflip {
	struct SWindowTarget {
		BOOL bDesktopWindow;
		HWND hWnd;
		WCHAR szTitle[256];
	};
}