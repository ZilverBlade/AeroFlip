#pragma once

#include <windef.h>

namespace aeroflip
{
	enum EWindowDrawObjectMoveMode
	{
		eWDOMM_DEFAULT = 0,
		eWDOMM_MOVING_TO_BACK = 1,

		eWDOMM_MAX_ENUM = 0x7fffffff
	};

	struct SWindowDrawObject
	{
		HWND hTargetWnd;
		FLOAT fPosition[3];
		FLOAT fRotationY;
		FLOAT fScale[3];
		FLOAT fOpacity;
		INT iZOrder;
		DWORD dwMoveMode;

		BOOL bFocused;
		BOOL bDecorated;
		BOOL bDesktopBg;
		RECT rcBounds;
	};
}