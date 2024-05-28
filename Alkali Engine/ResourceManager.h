#pragma once

#include "Application.h"

class ResourceManager
{
public:
	static void CreateCommittedResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource>& pDestinationResource, size_t numElements, size_t elementSize, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	static void UploadCommittedResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource>& pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData);
};

