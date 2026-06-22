#pragma once

#include <windef.h>

#include "ERendererMode.h"
#include "ETextureQuality.h"

namespace aeroflip
{
	struct SRendererConfig
	{
		BOOL bHardwareAcceleration;
		BOOL bLiveCapture;

		UINT uMultiSampleLevel;

		DWORD dwRendererMode;
		DWORD dwTextureQuality;

		void InitDefault()
		{
			bHardwareAcceleration = TRUE;
			bLiveCapture = FALSE;

			uMultiSampleLevel = 4;

			dwRendererMode = eRM_D3D9_EX;
			dwTextureQuality = eTQ_SMOOTH;
		}
	};
}