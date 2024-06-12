#pragma once

#include "pch.h"
#include "D3DClass.h"
#include "Shader.h"

struct VS_IN
{
	XMFLOAT3 Position;
	XMFLOAT3 Color;
};

class DebugLine
{
public:
	DebugLine(D3DClass* d3d, shared_ptr<Shader> shader, XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color);
	~DebugLine();

	void SetPositions(D3DClass* d3d, XMFLOAT3 start, XMFLOAT3 end);
	void SetEnabled(bool enabled);

	void InitializeDynamicVertexBuffer(D3DClass* d3d);

	void UpdateDynamicVertexBuffer(D3DClass* d3d);

	void Render(ID3D12GraphicsCommandList2* commandListDirect, ID3D12RootSignature* rootSig, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj);

private:
	bool m_enabled = true;

	XMFLOAT3 m_start, m_end;
	XMFLOAT3 m_color;

	shared_ptr<Shader> m_shader;

	ComPtr<ID3D12Resource> m_vertexBuffer;
	VS_IN* m_mappedVertexData;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};

