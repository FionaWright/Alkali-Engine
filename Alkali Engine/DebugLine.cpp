#include "pch.h"
#include "DebugLine.h"
#include "ResourceManager.h"
#include "CBuffers.h"

DebugLine::DebugLine(D3DClass* d3d, shared_ptr<Shader> shader, XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color)
	: m_start(start)
	, m_end(end)
	, m_color(color)
	, m_shader(shader)
{
	UpdateVertexBuffer(d3d);
}

DebugLine::~DebugLine()
{
}

void DebugLine::SetPositions(D3DClass* d3d, XMFLOAT3 start, XMFLOAT3 end)
{
	m_start = start;
	m_end = end;
	UpdateVertexBuffer(d3d);
}

void DebugLine::SetEnabled(bool enabled)
{
	m_enabled = enabled;
}

void DebugLine::UpdateVertexBuffer(D3DClass* d3d)
{
	struct VS_IN
	{
		XMFLOAT3 Position;
		XMFLOAT3 Color;
	};

	VS_IN vertices[] = {
		{ m_start, m_color},
		{ m_end, m_color }
	};

	// Create a vertex buffer
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(vertices);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	d3d->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer));

	// Copy data to the vertex buffer
	void* pVertexDataBegin;
	D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read from this resource on the CPU.
	m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
	memcpy(pVertexDataBegin, vertices, sizeof(vertices));
	m_vertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view
	m_vertexBufferView = {};
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(VS_IN);
	m_vertexBufferView.SizeInBytes = sizeof(vertices);
}

void DebugLine::Render(ID3D12GraphicsCommandList2* commandListDirect, ID3D12RootSignature* rootSig, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj)
{
	if (!m_enabled)
		return;

	commandListDirect->SetPipelineState(m_shader->GetPSO().Get());
	commandListDirect->SetGraphicsRootSignature(rootSig);
	commandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	commandListDirect->RSSetViewports(1, &viewPort);
	commandListDirect->RSSetScissorRects(1, &scissorRect);

	UINT numRenderTargets = 1;
	commandListDirect->OMSetRenderTargets(numRenderTargets, &rtv, FALSE, &dsv);

	commandListDirect->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	MatricesLineCB matricesCB;
	matricesCB.VP = viewProj;
	commandListDirect->SetGraphicsRoot32BitConstants(0, sizeof(MatricesLineCB) / 4, &matricesCB, 0);

	commandListDirect->DrawInstanced(2, 1, 0, 0);
}
