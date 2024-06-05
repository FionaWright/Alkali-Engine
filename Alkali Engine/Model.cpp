#include "pch.h"
#include "Model.h"
#include "ModelLoader.h"

Model::Model()
	:
	m_VertexBuffer(nullptr),
	m_IndexBuffer(nullptr),
	m_IndexBufferView({}),
	m_VertexBufferView({})
{
}

Model::~Model()
{
}

void Model::Init(ID3D12GraphicsCommandList2* commandList, wstring filepath)
{
	size_t vertexCount = 0, indexCount = 0;
	vector<VertexInputData> vertexBuffer;
	vector<int32_t> indexBuffer;

	ModelLoader::LoadModel(filepath, vertexBuffer, indexBuffer, vertexCount, indexCount);

	Init(commandList, vertexCount, indexCount, sizeof(VertexInputData));
	SetBuffers(commandList, vertexBuffer.data(), indexBuffer.data());
}

void Model::Init(ID3D12GraphicsCommandList2* commandList, string filepath)
{
	wstring wFilePath(filepath.begin(), filepath.end());
	Init(commandList, wFilePath);
}

void Model::Init(ID3D12GraphicsCommandList2* commandList, size_t vertexCount, size_t indexCount, size_t vertexInputSize)
{
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
	m_vertexInputSize = vertexInputSize;

	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	ResourceManager::CreateCommittedResource(commandList, m_VertexBuffer, m_vertexCount, m_vertexInputSize);

	m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexCount * m_vertexInputSize);
	m_VertexBufferView.StrideInBytes = static_cast<UINT>(m_vertexInputSize);

	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	ResourceManager::CreateCommittedResource(commandList, m_IndexBuffer, m_indexCount, sizeof(int32_t));

	m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_IndexBufferView.SizeInBytes = static_cast<UINT>(m_indexCount * sizeof(int32_t));
 }

void Model::SetBuffers(ID3D12GraphicsCommandList2* commandList, const void* vBufferData, const void* iBufferData)
{
	ResourceManager::UploadCommittedResource(commandList, m_VertexBuffer, &m_intermediateVertexBuffer, m_vertexCount, m_vertexInputSize, vBufferData);
	ResourceManager::UploadCommittedResource(commandList, m_IndexBuffer, &m_intermediateIndexBuffer, m_indexCount, sizeof(int32_t), iBufferData);
}

void Model::Render(ID3D12GraphicsCommandList2* commandList)
{
	commandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	commandList->IASetIndexBuffer(&m_IndexBufferView);

	commandList->DrawIndexedInstanced(static_cast<UINT>(m_indexCount), 1, 0, 0, 0);
}

size_t Model::GetVertexCount()
{
	return m_vertexCount;
}

size_t Model::GetIndexCount()
{
	return m_indexCount;
}
