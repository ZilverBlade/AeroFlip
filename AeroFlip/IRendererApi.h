#pragma once

#include <windef.h>

namespace aeroflip 
{
	struct IRendererApi
	{
		virtual ~IRendererApi() 
		{
		}
		virtual void UpdateWindows(class CWindowProvider* pWindowProvider) = NULL;
		virtual void ReleaseWindows() = NULL;
		virtual void OnRender(FLOAT fDeltaTime) = NULL;
	};
}