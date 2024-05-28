#include "pch.h"
#include "ResourceManager.h"

void ResourceManager::CreateCommittedResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource>& pDestinationResource, size_t numElements, size_t elementSize, D3D12_RESOURCE_FLAGS flags)
{
    size_t bufferSize = numElements * elementSize;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

    // Change nullptr
    ComPtr<ID3D12Device2> device = Application::Get().GetDevice();
    HRESULT hresult = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&pDestinationResource));

    ThrowIfFailed(hresult);
}

void ResourceManager::UploadCommittedResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource>& pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData)
{
    size_t bufferSize = numElements * elementSize;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    ComPtr<ID3D12Device2> device = Application::Get().GetDevice();
    HRESULT hresult = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(pIntermediateResource));

    ThrowIfFailed(hresult);

    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = bufferData;
    subresourceData.RowPitch = bufferSize;
    subresourceData.SlicePitch = subresourceData.RowPitch;

    UINT64 offset = 0;
    UINT startIndex = 0;
    UINT resourceCount = 1;
    UpdateSubresources(commandList.Get(), pDestinationResource.Get(), *pIntermediateResource, offset, startIndex, resourceCount, &subresourceData);
}
