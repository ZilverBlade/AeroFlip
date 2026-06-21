#pragma once

#include <windef.h>

namespace aeroflip 
{
	struct SWindowDrawObject 
	{
		HWND hTargetWnd;
		FLOAT fPosition[3];  
		FLOAT fRotationY;    
		FLOAT fScale[3];     
		FLOAT fOpacity; 
		INT iZOrder;
		BOOL bMovingToBack;

		BOOL bFocused;
		BOOL bDesktopBg;
	};
}