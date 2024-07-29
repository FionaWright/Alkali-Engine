#include "pch.h"
#include "DebugLine.h"
#include "ResourceManager.h"
#include "CBuffers.h"

DebugLine::DebugLine(D3DClass* d3d, RootParamInfo& rpi, shared_ptr<Shader> shader, shared_ptr<Material> material, XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color)
	: m_start(start)
	, m_end(end)
	, m_color(color)
	, m_shader(shader)
	, m_material(material)
	, m_rootParamInfo(rpi)
{
	InitializeDynamicVertexBuffer(d3d);
	UpdateDynamicVertexBuffer(d3d);
}

DebugLine::~DebugLine()
{
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Unmap(0, nullptr);
	}
}

void DebugLine::SetPositions(D3DClass* d3d, XMFLOAT3 start, XMFLOAT3 end)
{
	m_start = start;
	m_end = end;
	UpdateDynamicVertexBuffer(d3d);
}

void DebugLine::SetEnabled(bool enabled)
{
	m_enabled = enabled;
}

void DebugLine::InitializeDynamicVertexBuffer(D3DClass* d3d)
{
	// Create a vertex buffer
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(VS_IN) * 2; // Size for two vertices
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	d3d->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer));

	m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedVertexData));

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(VS_IN);
	m_vertexBufferView.SizeInBytes = sizeof(VS_IN) * 2;
}

void DebugLine::UpdateDynamicVertexBuffer(D3DClass* d3d)
{
	VS_IN vertices[] = {
		{ m_start, m_color},
		{ m_end, m_color }
	};

	memcpy(m_mappedVertexData, vertices, sizeof(vertices));
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

	m_material->SetCBV_PerDraw(0, &matricesCB, sizeof(MatricesLineCB));
	m_material->AssignMaterial(commandListDirect, m_rootParamInfo, -1);

	commandListDirect->DrawInstanced(2, 1, 0, 0);
}
