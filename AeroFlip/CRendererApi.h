#pragma once

#include <d3d9.h>

namespace aeroflip {
	struct SRendererApiConfig {
		HWND hWnd = NULL;
		BOOL bHardwareAcceleration = FALSE;
		UINT uMultiSampleLevel = 1;
	};

	class CRendererApi {
	public:
		CRendererApi(const SRendererApiConfig* pInConfig);
		~CRendererApi();
	
		void OnRender();
	private:
		void InitD3D9();
		void CreateD3D9Device(UINT uInAdapter, D3DDEVTYPE inDevType, HWND hInWnd, UINT uInMultiSampleLevel);

		UINT USelectD3D9HardwareAdapter(D3DDEVTYPE* pOutDevType);
		UINT USelectD3D9SoftwareAdapter(D3DDEVTYPE* pOutDevType);

		IDirect3D9Ex* m_pD3D9 = NULL;			
		IDirect3DDevice9Ex* m_pD3D9Device = NULL;	

		D3DPRESENT_PARAMETERS m_D3D9PresentParams;
	};
}