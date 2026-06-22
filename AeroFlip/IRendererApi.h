#pragma once

#include <windef.h>

namespace aeroflip 
{
	struct SRendererSettings
	{
		BOOL bLiveCapture;
		BOOL bRenderBorders;
	};

	struct IRendererApi
	{
		virtual ~IRendererApi() 
		{
		}
		virtual void UpdateWindows(class CWindowProvider* pWindowProvider) = NULL;
		virtual void ReleaseWindows() = NULL;
		virtual void OnRender(const struct SWindowDrawObject* pWindows, UINT cWindows) = NULL;
		virtual void SetSettings(const SRendererSettings* pSettings) = NULL;
	};
}