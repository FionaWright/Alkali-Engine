#pragma once

#include "Application.h"

class ResourceManager
{
public:
	static void CreateCommittedResource(ComPtr<ID3D12Resource>& pDestinationResource, size_t numElements, size_t elementSize, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	static void UploadCommittedResource(ID3D12GraphicsCommandList2* commandList, ComPtr<ID3D12Resource>& pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData);

	static ComPtr<ID3D12RootSignature> CreateRootSignature(CD3DX12_ROOT_PARAMETER1* params, UINT paramCount, D3D12_STATIC_SAMPLER_DESC* pSamplers, UINT samplerCount);

	static void TransitionResource(ID3D12GraphicsCommandList2* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState, UINT subresource = 4294967295U);

	static ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

	static ComPtr<ID3D12Device2> CreateDevice(IDXGIAdapter4* adapter);

	static ComPtr<IDXGIAdapter4> GetAdapter(bool bUseWarp);

	static bool CheckTearingSupport();
};

