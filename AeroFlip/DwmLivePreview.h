#pragma once

#include <windef.h>

// Modified from https://stackoverflow.com/a/13655211

#define DWMP_DESKTOP 1
#define DWMP_WINDOW 3

#pragma comment(lib, "dwmlivepreview.lib")
extern "C" __declspec(dllimport) UINT __stdcall DwmpActivateLivePreview(_In_ BOOL bSwitch, _In_opt_ HWND hWnd, _In_opt_ HWND hSrcWnd,
	_In_ DWORD dwFlags, HANDLE hPN0, INT iFI0);
