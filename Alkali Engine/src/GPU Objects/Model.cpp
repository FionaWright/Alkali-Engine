#include "pch.h"
#include "Model.h"
#include "ModelLoader.h"

Model::Model()
	: m_VertexBuffer(nullptr)
	, m_IndexBuffer(nullptr)
	, m_IndexBufferView({})
	, m_VertexBufferView({})
	, m_boundingSphereRadius(0)
	, m_centroid(XMFLOAT3_ZERO)
{
}

Model::~Model()
{
}

bool Model::Init(ID3D12GraphicsCommandList2* cmdList, wstring filepath)
{
	vector<VertexInputData> vertexBuffer;
	vector<int32_t> indexBuffer;
	float radius;
	XMFLOAT3 centroid;

	bool r = ModelLoader::LoadModel(filepath, vertexBuffer, indexBuffer, radius, centroid);
	if (!r)
		return false;

	Init(vertexBuffer.size(), indexBuffer.size(), sizeof(VertexInputData), radius, centroid);
	SetBuffers(cmdList, vertexBuffer.data(), indexBuffer.data());

	return true;
}

bool Model::Init(ID3D12GraphicsCommandList2* cmdList, string filepath)
{
	wstring wFilePath(filepath.begin(), filepath.end());
	return Init(cmdList, wFilePath);
}

void Model::Init(size_t vertexCount, size_t indexCount, size_t vertexInputSize, float boundingRadius, XMFLOAT3 centroid)
{
	m_boundingSphereRadius = boundingRadius;
	m_centroid = centroid;

	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
	m_vertexInputSize = vertexInputSize;

	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	ResourceManager::CreateCommittedResourceAsCommon(m_VertexBuffer, m_vertexCount, m_vertexInputSize);

	m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexCount * m_vertexInputSize);
	m_VertexBufferView.StrideInBytes = static_cast<UINT>(m_vertexInputSize);

	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	ResourceManager::CreateCommittedResourceAsCommon(m_IndexBuffer, m_indexCount, sizeof(int32_t));

	m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_IndexBufferView.SizeInBytes = static_cast<UINT>(m_indexCount * sizeof(int32_t));

	m_loadedData = true;
 }

void Model::SetBuffers(ID3D12GraphicsCommandList2* cmdList, const void* vBufferData, const void* iBufferData)
{
	ResourceManager::UploadCommittedResource(cmdList, m_VertexBuffer, &m_intermediateVertexBuffer, m_vertexCount, m_vertexInputSize, vBufferData);
	ResourceManager::UploadCommittedResource(cmdList, m_IndexBuffer, &m_intermediateIndexBuffer, m_indexCount, sizeof(int32_t), iBufferData);
}

void Model::Render(ID3D12GraphicsCommandList2* cmdList)
{
	if (!m_loadedData)
		return;

	cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	cmdList->IASetIndexBuffer(&m_IndexBufferView);

	cmdList->DrawIndexedInstanced(static_cast<UINT>(m_indexCount), 1, 0, 0, 0);
}

size_t Model::GetVertexCount()
{
	return m_vertexCount;
}

size_t Model::GetIndexCount()
{
	return m_indexCount;
}

float Model::GetSphereRadius()
{
	return m_boundingSphereRadius;
}

XMFLOAT3 Model::GetCentroid()
{
	return m_centroid;
}
