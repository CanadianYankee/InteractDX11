#include "stdafx.h"
#include "CubeSaver.h"


CCubeSaver::CCubeSaver(void)
{
	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&m_matWorld, I);
	XMStoreFloat4x4(&m_matView, I);
	XMStoreFloat4x4(&m_matProj, I);
}


CCubeSaver::~CCubeSaver(void)
{
}

BOOL CCubeSaver::InitSaverData()
{
//	m_InitTasks.run([this]() {
		CreateGeometryBuffers();
//	});

//	m_InitTasks.run([this]() {
		LoadShaders();
//	});

	return TRUE;
}

void CCubeSaver::CleanUpSaver()
{
	m_InitTasks.cancel();
	m_InitTasks.wait();
	
	if(m_pD3DContext)
	{
		m_pD3DContext->ClearState();
	}
}

BOOL CCubeSaver::OnResizeSaver()
{
	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PIDIV4, m_fAspectRatio, 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_matProj, P);

	return TRUE;
}

HRESULT CCubeSaver::CreateGeometryBuffers()
{
	HRESULT hr = S_OK;

	// Create vertex buffer
    Vertex vertices[] =
    {
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) }
    };

	D3D11_SUBRESOURCE_DATA vinitData = {0};
    vinitData.pSysMem = vertices;
	vinitData.SysMemPitch = 0;
	vinitData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC vbd(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
	hr = m_pD3DDevice->CreateBuffer(&vbd, &vinitData, &m_pCubeVB);

	if(SUCCEEDED(hr))
	{
		// Create the index buffer
		UINT indices[] = {
			// front face
			0, 1, 2,
			0, 2, 3,

			// back face
			4, 6, 5,
			4, 7, 6,

			// left face
			4, 5, 1,
			4, 1, 0,

			// right face
			3, 2, 6,
			3, 6, 7,

			// top face
			1, 5, 6,
			1, 6, 2,

			// bottom face
			4, 0, 3, 
			4, 3, 7
		};

		m_iIndexCount = ARRAYSIZE(indices);

		D3D11_SUBRESOURCE_DATA iinitData = {0};
		iinitData.pSysMem = indices;
		iinitData.SysMemPitch = 0;
		CD3D11_BUFFER_DESC ibd(sizeof(indices), D3D11_BIND_INDEX_BUFFER);
		hr = m_pD3DDevice->CreateBuffer(&ibd, &iinitData, &m_pCubeIB);
	}

	return hr;
}

HRESULT CCubeSaver::LoadShaders()
{
	HRESULT hr = S_OK;

	ComPtr<ID3D11DeviceChild> pShader;

	const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = 
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	VS_INPUTLAYOUTSETUP ILS;
	ILS.pInputDesc = vertexDesc;
	ILS.NumElements = ARRAYSIZE(vertexDesc);
	ILS.pInputLayout = NULL;
	hr = LoadShader(VertexShader, L"VertexShader.cso", nullptr, &pShader, &ILS);

	if(SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11VertexShader>(&m_pVertexShader);
		m_pInputLayout = ILS.pInputLayout;
		ILS.pInputLayout->Release();
	}

	if(SUCCEEDED(hr))
	{
		hr = LoadShader(PixelShader, L"PixelShader.cso", nullptr, &pShader);
		if(SUCCEEDED(hr))
		{
			hr = pShader.As<ID3D11PixelShader>(&m_pPixelShader);
		}
	}

	if(SUCCEEDED(hr))
	{
		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(cbPerObject), D3D11_BIND_CONSTANT_BUFFER);
		hr = m_pD3DDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pPerObject);
	}

	return hr;
}

BOOL CCubeSaver::PauseSaver()
{ 
	return TRUE;
}

BOOL CCubeSaver::ResumeSaver()
{
	return TRUE;
}

BOOL CCubeSaver::IterateSaver(float dt, float T)
{
	m_InitTasks.wait();

	BOOL bResult = UpdateScene(dt, T);
	if(bResult)
		bResult = RenderScene();

	return bResult;
}

BOOL CCubeSaver::UpdateScene(float dt, float T)
{
	float fRadius = 5.0f + 2.0f * sinf(T);
	float fPhi = (sinf(T * 1.243f) + 1.0f);
	float fTheta = sinf(0.1414f * T) * XM_PI;
	float x = fRadius * sinf(fPhi)*cosf(fTheta);
	float z = fRadius * sinf(fPhi)*sinf(fTheta);
	float y = fRadius * cosf(fPhi);

	// Build the view matrix.
	XMVECTOR pos    = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	target = XMVectorSetW(target, 1.0f);
	XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_matView, V);

	XMMATRIX W = XMLoadFloat4x4(&m_matWorld);
	XMMATRIX P = XMLoadFloat4x4(&m_matProj);

	XMMATRIX mWVP = XMMatrixTranspose(XMMatrixMultiply(XMMatrixMultiply(W, V), P));
	XMStoreFloat4x4(&m_PerObjectData.gWorldViewProj, mWVP);

	return TRUE;
}

BOOL CCubeSaver::RenderScene()
{
	HRESULT hr = S_OK;

	if(!m_pSwapChain || !m_pD3DContext) 
	{
		assert(false);
		return FALSE;
	}

	const float clrBackground[] = {0.098f, 0.098f, 0.439f, 1.000f};

	//D3D11_RASTERIZER_DESC nocullDesc;
	//ZeroMemory(&nocullDesc, sizeof(D3D11_RASTERIZER_DESC));
	//nocullDesc.FillMode = D3D11_FILL_SOLID;
	//nocullDesc.CullMode = D3D11_CULL_NONE;
	//nocullDesc.FrontCounterClockwise = false;
	//nocullDesc.DepthClipEnable = false;

	//ComPtr<ID3D11RasterizerState> pNoCull;
	//hr = m_pD3DDevice->CreateRasterizerState(&nocullDesc, &pNoCull);
	//m_pD3DContext->RSSetState(pNoCull.Get());
	
	m_pD3DContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clrBackground);
	m_pD3DContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_pD3DContext->IASetInputLayout(m_pInputLayout.Get());
	m_pD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	m_pD3DContext->IASetVertexBuffers(0, 1, m_pCubeVB.GetAddressOf(), &stride, &offset);
	m_pD3DContext->IASetIndexBuffer(m_pCubeIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	m_pD3DContext->UpdateSubresource(m_pPerObject.Get(), 0, NULL, &m_PerObjectData, 0, 0);

	m_pD3DContext->VSSetShader(m_pVertexShader.Get(), NULL, 0);
	m_pD3DContext->VSSetConstantBuffers(0, 1, m_pPerObject.GetAddressOf());
	m_pD3DContext->PSSetShader(m_pPixelShader.Get(), NULL, 0);

	m_pD3DContext->DrawIndexed(m_iIndexCount, 0, 0);

	hr = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(hr);
}
