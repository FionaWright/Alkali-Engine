#include "pch.h"
#include "Model.h"
#include "ModelLoader.h"

Model::Model()
{
}

Model::~Model()
{
}

void Model::Init(ComPtr<ID3D12GraphicsCommandList2> commandList, wstring filepath)
{
	size_t vertexCount = 0, indexCount = 0;
	vector<VertexInputData> vertexBuffer;
	vector<WORD> indexBuffer;

	ModelLoader::LoadModel(filepath, vertexBuffer, indexBuffer, vertexCount, indexCount);

	Init(commandList, vertexCount, indexCount, sizeof(VertexInputData));
	SetBuffers(commandList, vertexBuffer.data(), indexBuffer.data());
}

void Model::Init(ComPtr<ID3D12GraphicsCommandList2> commandList, size_t vertexCount, size_t indexCount, size_t vertexInputSize)
{
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
	m_vertexInputSize = vertexInputSize;

	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	ResourceManager::CreateCommittedResource(commandList, m_VertexBuffer, m_vertexCount, m_vertexInputSize);

	m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = m_vertexCount * m_vertexInputSize;
	m_VertexBufferView.StrideInBytes = m_vertexInputSize;

	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	ResourceManager::CreateCommittedResource(commandList, m_IndexBuffer, m_indexCount, sizeof(WORD));

	m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_IndexBufferView.SizeInBytes = m_indexCount * sizeof(WORD);
 }

void Model::SetBuffers(ComPtr<ID3D12GraphicsCommandList2> commandList, const void* vBufferData, const void* iBufferData)
{
	ResourceManager::UploadCommittedResource(commandList, m_VertexBuffer, &m_intermediateVertexBuffer, m_vertexCount, m_vertexInputSize, vBufferData);
	ResourceManager::UploadCommittedResource(commandList, m_IndexBuffer, &m_intermediateIndexBuffer, m_indexCount, sizeof(WORD), iBufferData);
}

void Model::Render(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	commandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	commandList->IASetIndexBuffer(&m_IndexBufferView);

	commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}
