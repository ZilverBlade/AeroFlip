#include "stdafx.h"
#include "CWindowProvider.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <vector>

// Windows 8+ only
#define DWMWA_CLOAKED 13

namespace aeroflip
{
	BOOL IsWindows8OrGreater()
	{
		OSVERSIONINFOEXW osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXW));
		osvi.dwOSVersionInfoSize = sizeof(osvi);
		osvi.dwMajorVersion = 6;
		osvi.dwMinorVersion = 2; // 6.2 is Windows 8

		DWORDLONG dwlConditionMask = 0;
		VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
		VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

		return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask) != FALSE;
	}

	struct SEnumContext
	{
		CWindowProvider* pThis;
		std::vector<SWindowTarget>* pList;
		const WCHAR* szIgnoreClass;
	};

	// Callback filter logic
	BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
	{
		const SEnumContext* pContext = (const SEnumContext*)lParam;

		// Desktop window is special
		if (hWnd == GetShellWindow())
		{
			SWindowTarget target;
			ZeroMemory(&target, sizeof(SWindowTarget));
			target.bDesktopWindow = TRUE;
			target.hWnd = hWnd;
			target.bNeedsUpdate = TRUE;
			wcscpy_s(target.szTitle, L"Desktop");
			pContext->pList->push_back(target);
			return TRUE;
		}

		if (!IsWindowVisible(hWnd) || !IsWindowEnabled(hWnd))
		{
			return TRUE;
		}

		if (IsWindows8OrGreater())
		{
			int cloaked = 0;
			DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
			if (cloaked)
			{
				return TRUE;
			}
		}
		LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_TOOLWINDOW)
		{
			return TRUE;
		}

		HWND hOwner = GetWindow(hWnd, GW_OWNER);
		if (hOwner != NULL)
		{
			return TRUE;
		}
		WCHAR szTitle[256];
		ZeroMemory(szTitle, sizeof(szTitle));
		GetWindowText(hWnd, szTitle, 256);
		if (wcslen(szTitle) == 0)
		{
			return TRUE;
		}
		WCHAR szClassName[256];
		ZeroMemory(szClassName, sizeof(szClassName));
		GetClassName(hWnd, szClassName, 256);
		if (wcsncmp(szClassName, pContext->szIgnoreClass, 256) == 0)
		{
			return TRUE;
		}
		if (wcscmp(szClassName, L"Windows.UI.Core.CoreWindow") == 0 ||
			wcscmp(szClassName, L"Shell_TrayWnd") == 0 ||
			wcscmp(szClassName, L"Progman") == 0)
		{
			return TRUE;
		}

		SWindowTarget target;
		ZeroMemory(&target, sizeof(SWindowTarget));
		target.hWnd = hWnd;
		wcscpy_s(target.szTitle, szTitle);
		target.bMinimized = IsIconic(hWnd);
		target.bNeedsUpdate = TRUE;
		pContext->pList->push_back(target);

		return TRUE;
	}

	CWindowProvider::CWindowProvider(const SWindowProviderConfig* pConfig)
		: m_hWindow(pConfig->hWnd), m_szAppWindowClass(pConfig->szAppWindowClass)
	{
	}

	CWindowProvider::~CWindowProvider()
	{
		for (auto& wndTarget : m_ActiveWindows)
		{
			if (wndTarget.pCachedPixels)
			{
				free(wndTarget.pCachedPixels);
			}
		}
	}

	void CWindowProvider::UpdateWindowList()
	{
		std::vector<SWindowTarget> oldWindows = m_ActiveWindows;
		m_ActiveWindows.clear();

		SEnumContext context;
		context.pThis = this;
		context.pList = &m_ActiveWindows;
		context.szIgnoreClass = m_szAppWindowClass;
		EnumWindows(EnumWindowsProc, (LPARAM)&context);

		for (auto& newTarget : m_ActiveWindows)
		{
			for (auto& oldTarget : oldWindows)
			{
				if (newTarget.hWnd == oldTarget.hWnd)
				{
					newTarget.bNeedsUpdate = oldTarget.bNeedsUpdate;
					newTarget.bMinimized = oldTarget.bMinimized;
					newTarget.uCachedWidth = oldTarget.uCachedWidth;
					newTarget.uCachedHeight = oldTarget.uCachedHeight;
					newTarget.pCachedPixels = oldTarget.pCachedPixels;
					newTarget.cbCachedPixels = oldTarget.cbCachedPixels;
					newTarget.rcCachedBounds = oldTarget.rcCachedBounds;
					newTarget.bHasCachedBounds = oldTarget.bHasCachedBounds;

					oldTarget.pCachedPixels = NULL;
					break;
				}
			}
		}

		for (auto& oldTarget : oldWindows)
		{
			if (oldTarget.pCachedPixels != NULL)
			{
				free(oldTarget.pCachedPixels);
				oldTarget.pCachedPixels = NULL;
			}
		}

		FlagActiveWindow(m_hLastActiveWindow);
	}

	void CWindowProvider::FlagActiveWindow(HWND hWnd)
	{
		for (auto& wndTarget : m_ActiveWindows)
		{
			wndTarget.bActive = wndTarget.hWnd == hWnd;
		}
		m_hLastActiveWindow = hWnd;
	}

	HWND CWindowProvider::HGetActiveWindow()
	{
		return m_hLastActiveWindow;
	}

	void CWindowProvider::SetWindowMinimizedFlag(HWND hWnd, BOOL bMinimized)
	{
		for (auto& target : m_ActiveWindows)
		{
			if (target.hWnd == hWnd)
			{
				target.bMinimized = bMinimized;
				break;
			}
		}
	}

	void CWindowProvider::InvalidateWindow(HWND hWnd)
	{
		for (auto& target : m_ActiveWindows)
		{
			if (target.hWnd == hWnd)
			{
				target.bNeedsUpdate = TRUE;
				break;
			}
		}
	}

	void CWindowProvider::MarkWindowUpdated(HWND hWnd)
	{
		for (auto& target : m_ActiveWindows)
		{
			if (target.hWnd == hWnd)
			{
				target.bNeedsUpdate = FALSE;
				break;
			}
		}
	}

	void CWindowProvider::ClearWindowCache(HWND hWnd)
	{
		for (auto& target : m_ActiveWindows)
		{
			if (target.hWnd == hWnd)
			{
				if (target.pCachedPixels)
				{
					free(target.pCachedPixels);
					target.pCachedPixels = NULL;
				}
				target.bMinimized = FALSE;
				break;
			}
		}
	}

	void CWindowProvider::InvalidateAllWindows()
	{
		for (auto& target : m_ActiveWindows)
		{
			target.bNeedsUpdate = TRUE;
		}
	}

	void CWindowProvider::QueryWindows(const SWindowTarget** ppWindowTargets, UINT* pCount)
	{
		*pCount = (UINT)m_ActiveWindows.size();
		if (ppWindowTargets) *ppWindowTargets = m_ActiveWindows.data();
	}


	void CWindowProvider::CacheWindowThumbnail(HWND hWnd)
	{
		for (auto& target : m_ActiveWindows)
		{

			if (target.hWnd == hWnd)
			{
				RECT rect;

				if (IsIconic(hWnd))
				{
					if (target.bHasCachedBounds)
					{
						rect = target.rcCachedBounds;
					}
					else
					{
						// Fallback
						WINDOWPLACEMENT wp;
						wp.length = sizeof(WINDOWPLACEMENT);
						GetWindowPlacement(hWnd, &wp);
						rect = wp.rcNormalPosition;
					}
				}
				else
				{
					GetClientRect(hWnd, &rect);
					target.rcCachedBounds = rect;
					target.bHasCachedBounds = TRUE;
				}

				int width = rect.right - rect.left;
				int height = rect.bottom - rect.top;

				if (width <= 0 || height <= 0) return;

				HDC hdcScreen = GetDC(NULL);
				HDC hdcMem = CreateCompatibleDC(hdcScreen);
				HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);

				HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);
				PrintWindow(hWnd, hdcMem, 3);

				SelectObject(hdcMem, hOld);

				target.uCachedWidth = (UINT)width;
				target.uCachedHeight = (UINT)height;

				int rowBytes = width * 4;
				UINT newSize = (UINT)rowBytes * height;

				if (target.cbCachedPixels != newSize || target.pCachedPixels == NULL)
				{
					if (target.pCachedPixels) free(target.pCachedPixels);
					target.pCachedPixels = (BYTE*)malloc(newSize);
					target.cbCachedPixels = newSize;
				}

				if (target.pCachedPixels)
				{
					BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB };
					GetDIBits(hdcMem, hBitmap, 0, height, target.pCachedPixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
				}

				DeleteObject(hBitmap);
				DeleteDC(hdcMem);
				ReleaseDC(NULL, hdcScreen);
				break;
			}
		}
	}

	BOOL CWindowProvider::BWindowHasCache(HWND hWnd)
	{
		for (auto& target : m_ActiveWindows)
		{
			if (target.hWnd == hWnd)
			{
				return target.pCachedPixels != NULL;
			}
		}
		return FALSE;
	}

	void CWindowProvider::ClearWindowList()
	{
		for (auto& target : m_ActiveWindows)
		{
			if (target.pCachedPixels)
			{
				free(target.pCachedPixels);
			}
		}
	}
}
