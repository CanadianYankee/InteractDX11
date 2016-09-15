#include "stdafx.h"
#include "Cube.h"


CCube::CCube(void) :
	m_4xMsaaQuality(0),
	m_bEnable4xMsaa(false)
{
}


CCube::~CCube(void)
{
}

BOOL CCube::InitSaverData()
{
	HRESULT hr = S_OK;

	hr = InitDirect3D();

	if(SUCCEEDED(hr))
	{
		OnResize();
	}

	return SUCCEEDED(hr);
}

void CCube::CleanUpSaver()
{
	if(m_pD3DContext)
	{
		m_pD3DContext->ClearState();
	}
}

// Look up the DXGI adapter based on the hosting HWND
HRESULT CCube::GetMyAdapter(IDXGIAdapter **ppAdapter)
{
	HRESULT hr = S_OK;
	*ppAdapter = nullptr;
	bool bFound = false;

	assert(m_hMyWindow);
	HMONITOR hMonitor = MonitorFromWindow(m_hMyWindow, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFOEX monitorInfo;
	ZeroMemory(&monitorInfo, sizeof(monitorInfo));
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);

	ComPtr<IDXGIAdapter> pAdapter;
	ComPtr<IDXGIFactory> pFactory;
	hr = CreateDXGIFactory(__uuidof(IDXGIFactory), &pFactory);
	if(SUCCEEDED(hr))
	{
		for(UINT iAdapter = 0; !bFound; iAdapter++)
		{
			// When this call fails, there are no more adapters left
			HRESULT hrIter = pFactory->EnumAdapters(iAdapter, &pAdapter);
			if(FAILED(hrIter)) break;


			for(UINT iOutput = 0; !bFound; iOutput++)
			{
				ComPtr<IDXGIOutput> pOutput;
				hrIter = pAdapter->EnumOutputs(iOutput, &pOutput);
				if(FAILED(hrIter)) break;

				DXGI_OUTPUT_DESC outputDesc;
				hr = pOutput->GetDesc(&outputDesc);
				if(SUCCEEDED(hr) && (_tcscmp(outputDesc.DeviceName, monitorInfo.szDevice) == 0))
				{
					bFound = true;
					*ppAdapter = pAdapter.Detach();
				}
			}
		}
	}

	if(SUCCEEDED(hr))
		return bFound ? S_OK : E_FAIL;
	else
		return hr;
}

// Create D3D device, context, and swap chain
HRESULT CCube::InitDirect3D()
{
	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	ComPtr<IDXGIAdapter> pAdapter;
	hr = GetMyAdapter(&pAdapter);
	if(SUCCEEDED(hr))
	{
		D3D_FEATURE_LEVEL featureLevel;
		hr = D3D11CreateDevice(pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL,
			createDeviceFlags, NULL, 0, D3D11_SDK_VERSION, &m_pD3DDevice, &featureLevel,
			&m_pD3DContext);

		if(SUCCEEDED(hr))
		{
			if(featureLevel != D3D_FEATURE_LEVEL_11_0) 
				hr = E_FAIL;
		}
		assert(SUCCEEDED(hr));
	}

	if(SUCCEEDED(hr))
	{
		// Check 4X MSAA quality support for our back buffer format.
		// All Direct3D 11 capable devices support 4X MSAA for all render 
		// target formats, so we only need to check quality support.
		hr = m_pD3DDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m_4xMsaaQuality);
		assert( SUCCEEDED(hr) && m_4xMsaaQuality > 0 );
	}

	if(SUCCEEDED(hr))
	{
		// Fill out a DXGI_SWAP_CHAIN_DESC to describe our swap chain.
		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width  = m_iClientWidth;
		sd.BufferDesc.Height = m_iClientHeight;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		// Use 4X MSAA? 
		if( m_bEnable4xMsaa && m_4xMsaaQuality > 0)
		{
			sd.SampleDesc.Count   = 4;
			sd.SampleDesc.Quality = m_4xMsaaQuality-1;
		}
		// No MSAA
		else
		{
			sd.SampleDesc.Count   = 1;
			sd.SampleDesc.Quality = 0;
		}

		sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount  = 1;
		sd.OutputWindow = m_hMyWindow;
		sd.Windowed     = TRUE;
		sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags        = 0;

		ComPtr<IDXGIFactory> dxgiFactory;
		hr = pAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory);

		if(SUCCEEDED(hr))
		{
			hr = dxgiFactory->CreateSwapChain(m_pD3DDevice.Get(), &sd, &m_pSwapChain);
		}

		if(SUCCEEDED(hr))
		{
			hr = dxgiFactory->MakeWindowAssociation(m_hMyWindow, DXGI_MWA_NO_WINDOW_CHANGES);
		}
	}

	return hr;
}

// Create/recreate the depth/stencil view and render target based on new size
BOOL CCube::OnResize()
{
	HRESULT hr = S_OK;

	if(m_pSwapChain)
	{
		// Release the old views, as they hold references to the buffers we
		// will be destroying.  Also release the old depth/stencil buffer.
		m_pRenderTargetView = nullptr;
		m_pDepthStencilView = nullptr;
		m_pDepthStencilBuffer = nullptr;

		// Resize the swap chain and recreate the render target view.
		hr = m_pSwapChain->ResizeBuffers(1, m_iClientWidth, m_iClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		if(SUCCEEDED(hr))
		{
			ComPtr<ID3D11Texture2D> pBackBuffer;
			hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer);
			if(SUCCEEDED(hr))
			{
				hr = m_pD3DDevice->CreateRenderTargetView(pBackBuffer.Get(), 0, &m_pRenderTargetView);
			}
		}

		if(SUCCEEDED(hr))
		{
			// Create the depth/stencil buffer and view.
			D3D11_TEXTURE2D_DESC depthStencilDesc;
	
			depthStencilDesc.Width     = m_iClientWidth;
			depthStencilDesc.Height    = m_iClientHeight;
			depthStencilDesc.MipLevels = 1;
			depthStencilDesc.ArraySize = 1;
			depthStencilDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;

			// Use 4X MSAA? --must match swap chain MSAA values.
			if( m_bEnable4xMsaa )
			{
				depthStencilDesc.SampleDesc.Count   = 4;
				depthStencilDesc.SampleDesc.Quality = m_4xMsaaQuality-1;
			}
			// No MSAA
			else
			{
				depthStencilDesc.SampleDesc.Count   = 1;
				depthStencilDesc.SampleDesc.Quality = 0;
			}

			depthStencilDesc.Usage          = D3D11_USAGE_DEFAULT;
			depthStencilDesc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;
			depthStencilDesc.CPUAccessFlags = 0; 
			depthStencilDesc.MiscFlags      = 0;

			hr = m_pD3DDevice->CreateTexture2D(&depthStencilDesc, 0, &m_pDepthStencilBuffer);
			if(SUCCEEDED(hr))
			{
				hr = m_pD3DDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), 0, &m_pDepthStencilView);
			}
		}

		if(SUCCEEDED(hr))
		{
			// Bind the render target view and depth/stencil view to the pipeline.
			m_pD3DContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());
	
			// Set the viewport transform.
			m_ScreenViewport.TopLeftX = 0;
			m_ScreenViewport.TopLeftY = 0;
			m_ScreenViewport.Width    = static_cast<float>(m_iClientWidth);
			m_ScreenViewport.Height   = static_cast<float>(m_iClientHeight);
			m_ScreenViewport.MinDepth = 0.0f;
			m_ScreenViewport.MaxDepth = 1.0f;

			m_pD3DContext->RSSetViewports(1, &m_ScreenViewport);
		}
	}

	assert(SUCCEEDED(hr));

	return TRUE;
}

BOOL CCube::PauseSaver()
{ 
	return TRUE;
}

BOOL CCube::ResumeSaver()
{
	return TRUE;
}

BOOL CCube::IterateSaver(SAVERFLOAT dt, SAVERFLOAT T)
{
	BOOL bResult = UpdateScene(dt, T);
	if(bResult)
		bResult = RenderScene();

	return bResult;
}

BOOL CCube::UpdateScene(float dt, float T)
{
	float R = 0.5f*(sinf(T) + 1.0f);
	float G = 0.5f*(sinf(T * 1.73205f) + 1.0f);
	float B = 0.5f*(sinf(T * 2.236067f) + 1.0f);

	m_clrBackground[0] = R;
	m_clrBackground[1] = G;
	m_clrBackground[2] = B;
	m_clrBackground[3] = 1.0f;

	return TRUE;
}

BOOL CCube::RenderScene()
{
	HRESULT hr = S_OK;

	if(!m_pSwapChain || !m_pD3DContext) 
	{
		assert(false);
		return FALSE;
	}
	
	m_pD3DContext->ClearRenderTargetView(m_pRenderTargetView.Get(), m_clrBackground);
	m_pD3DContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	hr = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(hr);
}
