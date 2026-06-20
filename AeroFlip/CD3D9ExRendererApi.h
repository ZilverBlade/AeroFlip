#pragma once

#include "IRendererApi.h"

#include <d3d9.h>
#include <unordered_map>

namespace aeroflip
{
	struct SD3D9ExRendererApiConfig
	{
		HWND hWnd;
		BOOL bHardwareAcceleration;
		UINT uMultiSampleLevel;
	};

	class CD3D9ExRendererApi : public IRendererApi
	{
	public:
		CD3D9ExRendererApi(const SD3D9ExRendererApiConfig* pConfig);
		~CD3D9ExRendererApi();

		void UpdateWindows(class CWindowProvider* pWindowProvider);
		void ReleaseWindows();
		void OnRender(const struct SWindowDrawObject* pWindows, UINT cWindows);
	private:
		void InitD3D9Ex();
		void CreateD3D9ExDevice(UINT uAdapter, D3DDEVTYPE devType, HWND hWnd, UINT uMultiSampleLevel);

		UINT USelectD3D9ExHardwareAdapter(D3DDEVTYPE* pOutDevType);
		UINT USelectD3D9ExSoftwareAdapter(D3DDEVTYPE* pOutDevType);

		void ResetD3D9ExDevice();
	private:
		IDirect3D9Ex* m_pD3D9Ex = NULL;
		IDirect3DDevice9Ex* m_pD3D9ExDevice = NULL;

		D3DPRESENT_PARAMETERS m_D3DPresentParams;

		HWND m_hWindow = NULL;

		struct SWindowTexturePair
		{
			BOOL bIsFocused;
			HWND hWnd;
			IDirect3DTexture9* pD3D9Texture;
		};

		std::vector<SWindowTexturePair> m_WindowTextureList;
	};
}