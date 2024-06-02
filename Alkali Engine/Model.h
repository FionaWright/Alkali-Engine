#pragma once

#include "Application.h"
#include "ResourceManager.h"

#include <string>

using std::string;
using std::wstring;

class Model
{
public:
	Model();
    ~Model();

    void Init(ComPtr<ID3D12GraphicsCommandList2> commandList, wstring filepath);
    void Init(ComPtr<ID3D12GraphicsCommandList2> commandList, size_t vertexCount, size_t indexCount, size_t vertexInputSize);

    void SetBuffers(ComPtr<ID3D12GraphicsCommandList2> commandList, const void* vBufferData, const void* iBufferData);
    void Render(ComPtr<ID3D12GraphicsCommandList2> commandList);

    size_t GetVertexCount();
    size_t GetIndexCount();

private:
    ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;

    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

    ComPtr<ID3D12Resource> m_intermediateVertexBuffer;
    ComPtr<ID3D12Resource> m_intermediateIndexBuffer;

    size_t m_vertexCount = 0, m_indexCount = 0, m_vertexInputSize = 0;
};