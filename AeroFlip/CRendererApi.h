#pragma once

#include <d3d9.h>
#include <unordered_map>

namespace aeroflip {
	struct SRendererApiConfig {
		HWND hWnd;
		BOOL bHardwareAcceleration;
		UINT uMultiSampleLevel;
	};

	class CRendererApi {
	public:
		CRendererApi(const SRendererApiConfig* pConfig);
		~CRendererApi();
		
		void UpdateWindows(class CWindowProvider* pWindowProvider);
		void OnRender(FLOAT fDeltaTime);
	private:
		void InitD3D9();
		void CreateD3D9Device(UINT uAdapter, D3DDEVTYPE devType, HWND hWnd, UINT uMultiSampleLevel);

		UINT USelectD3D9HardwareAdapter(D3DDEVTYPE* pOutDevType);
		UINT USelectD3D9SoftwareAdapter(D3DDEVTYPE* pOutDevType);

		void ResetD3D9Device();
	private:
		IDirect3D9Ex* m_pD3D9 = NULL;			
		IDirect3DDevice9Ex* m_pD3D9Device = NULL;	

		D3DPRESENT_PARAMETERS m_D3D9PresentParams;

		HWND m_hWindow = NULL;

		std::unordered_map<HWND, IDirect3DTexture9*> m_WindowTextureMap;
	};
}