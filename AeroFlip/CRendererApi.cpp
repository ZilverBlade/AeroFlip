#include "stdafx.h"
#include "CRendererApi.h"

#include <stdexcept>

namespace aeroflip {
	CRendererApi::CRendererApi(const SRendererApiConfig* pInConfig) {
		InitD3D9();
		UINT uAdapter;
		D3DDEVTYPE devType;
		if (pInConfig->bHardwareAcceleration) {
			uAdapter = USelectD3D9HardwareAdapter(&devType);
		}
		else {
			uAdapter = USelectD3D9SoftwareAdapter(&devType);
		}

		CreateD3D9Device(uAdapter, devType, pInConfig->hWnd, pInConfig->uMultiSampleLevel);
	}
	CRendererApi::~CRendererApi() {
		SafeRelease(m_pD3D9Device);
		SafeRelease(m_pD3D9);
	}

	void CRendererApi::OnRender() {
		// TODO
	}

	void CRendererApi::InitD3D9() {
		if (!SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9))) {
			throw std::runtime_error("Failed to init Direct3D9!");
		}
	}

	void CRendererApi::CreateD3D9Device(UINT uInAdapter, D3DDEVTYPE inDevType, HWND hInWnd, UINT uInMultiSampleLevel) {
		DWORD dwFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

		ZeroMemory(&m_D3D9PresentParams, sizeof(D3DPRESENT_PARAMETERS));
		m_D3D9PresentParams.hDeviceWindow = hInWnd;
		m_D3D9PresentParams.Windowed = TRUE;
		m_D3D9PresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
		m_D3D9PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)uInMultiSampleLevel;
		m_D3D9PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		m_D3D9PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;


		HRESULT hr = m_pD3D9->CreateDeviceEx(uInAdapter, inDevType, hInWnd, dwFlags,
			&m_D3D9PresentParams,
			NULL,
			&m_pD3D9Device);

		if (SUCCEEDED(hr))
		{
			D3DCAPS9 Caps;
			ZeroMemory(&Caps, sizeof(D3DCAPS9));
			m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &Caps);

			// Skip backbuffer formats that don't support alpha blending
			hr = m_pD3D9->CheckDeviceFormat(Caps.AdapterOrdinal, Caps.DeviceType,
				m_D3D9PresentParams.BackBufferFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
				D3DRTYPE_TEXTURE, m_D3D9PresentParams.BackBufferFormat);
			if (FAILED(hr))
			{
				throw std::runtime_error("Failed to query alpha-capable backbuffer!");
			}
		}

	}

	UINT CRendererApi::USelectD3D9HardwareAdapter(D3DDEVTYPE* pOutDevType) {
		OutputDebugString(L"Selecting D3D9 Hardware Accelerated Adapter\n");
		UINT uNumAdapters = m_pD3D9->GetAdapterCount();
		UINT uSelectedAdapter = D3DADAPTER_DEFAULT;
		BOOL bFoundHardware = FALSE;

		for (UINT uAdapter = 0; uAdapter < uNumAdapters; ++uAdapter) {
			D3DCAPS9 caps;
			HRESULT hr = m_pD3D9->GetDeviceCaps(uAdapter, D3DDEVTYPE_HAL, &caps);

			if (SUCCEEDED(hr)) {
				uSelectedAdapter = uAdapter;
				bFoundHardware = true;
				OutputDebugString(L"Found D3D9 Hardware Accelerated Adapter\n");
				break;
			}
		}
			
		if (!bFoundHardware) {
			OutputDebugString(L"No hardware adapter found. Falling back to software/REF.\n");
			return USelectD3D9SoftwareAdapter(pOutDevType);
		}
		*pOutDevType = D3DDEVTYPE_HAL;
		return uSelectedAdapter;
	}

	UINT CRendererApi::USelectD3D9SoftwareAdapter(D3DDEVTYPE* pOutDevType) {
		UINT uNumAdapters = m_pD3D9->GetAdapterCount();

		for (UINT uAdapter = 0; uAdapter < uNumAdapters; ++uAdapter) {
			D3DCAPS9 caps;
			HRESULT hr = m_pD3D9->GetDeviceCaps(uAdapter, D3DDEVTYPE_REF, &caps);

			if (SUCCEEDED(hr)) {
				OutputDebugString(L"Successfully selected D3D9 Software (Reference) Adapter.\n");
				*pOutDevType = D3DDEVTYPE_REF;
				return uAdapter;
			}
		}

		throw std::runtime_error("Failed to find an adapter supporting the Reference Rasterizer!");
	}

}