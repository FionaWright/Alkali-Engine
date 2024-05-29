#pragma once

#include "Application.h"

class ResourceManager
{
public:
	static void CreateCommittedResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource>& pDestinationResource, size_t numElements, size_t elementSize, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	static void UploadCommittedResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource>& pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData);

	static ComPtr<ID3D12RootSignature> CreateRootSignature(CD3DX12_ROOT_PARAMETER1* params, UINT paramCount);

	static void TransitionResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	static ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
};

