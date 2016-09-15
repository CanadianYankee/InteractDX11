#pragma once

#include "saverbase.h"

struct cbPerObject
{
	XMFLOAT4X4 gWorldViewProj;
};

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

class CCubeSaver :
	public CSaverBase
{
public:
	CCubeSaver(void);
	virtual ~CCubeSaver(void);

protected:
	virtual BOOL InitSaverData();
	virtual BOOL IterateSaver(float dt, float T);
	virtual BOOL PauseSaver();
	virtual BOOL ResumeSaver();
	virtual BOOL OnResizeSaver();
	virtual void CleanUpSaver();

protected:
	virtual BOOL UpdateScene(float dt, float T);
	virtual BOOL RenderScene();

	HRESULT CreateGeometryBuffers();
	HRESULT LoadShaders();

	concurrency::task_group m_InitTasks;

	ComPtr<ID3D11VertexShader> m_pVertexShader;
	ComPtr<ID3D11PixelShader> m_pPixelShader;
	ComPtr<ID3D11InputLayout> m_pInputLayout;
	cbPerObject m_PerObjectData;

	XMFLOAT4X4 m_matWorld;
	XMFLOAT4X4 m_matView;
	XMFLOAT4X4 m_matProj;

	ComPtr<ID3D11Buffer> m_pPerObject;
	ComPtr<ID3D11Buffer> m_pCubeVB;
	ComPtr<ID3D11Buffer> m_pCubeIB;
	UINT m_iIndexCount;
};

