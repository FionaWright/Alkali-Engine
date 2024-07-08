#include "pch.h"
#include "DescriptorManager.h"

unordered_map<string, UINT> DescriptorManager::ms_descriptorIndexMap;
ComPtr<ID3D12DescriptorHeap> DescriptorManager::ms_srv_cbv_uav_Heap;
size_t DescriptorManager::ms_nextDescriptorIndex;
UINT DescriptorManager::ms_descriptorIncrementSize;
bool DescriptorManager::ms_initialised;
vector<string> DescriptorManager::ms_debugHeapList;
bool DescriptorManager::ms_DebugHeapEnabled;

void DescriptorManager::Init(D3DClass* d3d, UINT numDescriptors)
{
	ms_nextDescriptorIndex = 0;
	ms_descriptorIncrementSize = d3d->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ms_srv_cbv_uav_Heap = ResourceManager::CreateDescriptorHeap(numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	ms_initialised = true;
	ms_DebugHeapEnabled = DEBUG_HEAP_ENABLED_DEFAULT;
}

UINT DescriptorManager::AddSRVs(D3DClass* d3d, const vector<shared_ptr<Texture>>& textures)
{
	if (!ms_initialised)
		throw std::exception("Uninitialised Descriptor Manager");

	string id = "";
	for (size_t i = 0; i < textures.size(); i++)
	{
		if (i != 0)
			id += "~";

		id += textures[i]->GetFilePath();
	}

	if (ms_descriptorIndexMap.contains(id))
		return ms_descriptorIndexMap.at(id);

	UINT heapStart = static_cast<UINT>(ms_nextDescriptorIndex);
	ms_nextDescriptorIndex += textures.size();

	for (size_t i = 0; i < textures.size(); i++)
	{
		textures[i]->AddToDescriptorHeap(d3d, ms_srv_cbv_uav_Heap.Get(), heapStart + i);

		if (ms_DebugHeapEnabled)
			ms_debugHeapList.push_back("SRV: " + textures[i]->GetFilePath());
	} 

	ms_descriptorIndexMap.emplace(id, heapStart);
	return heapStart;
}

UINT DescriptorManager::AddDynamicSRVs(UINT count)
{
	if (!ms_initialised)
		throw std::exception("Uninitialised Descriptor Manager");

	if (ms_DebugHeapEnabled)
	{
		for (UINT i = 0; i < count; i++)
			ms_debugHeapList.push_back("Dynamic SRV: " + std::to_string(i));
	}

	UINT heapStart = static_cast<UINT>(ms_nextDescriptorIndex);
	ms_nextDescriptorIndex += count;
	return heapStart;
}

UINT DescriptorManager::AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes, vector<ID3D12Resource*>& cbvResources, bool sharing)
{
	if (!ms_initialised)
		throw std::exception("Uninitialised Descriptor Manager");

	string id = "";
	for (size_t i = 0; i < sizes.size(); i++)
	{
		if (i != 0)
			id += "~";

		id += std::to_string(sizes[i]);
	}

	if (sharing && ms_descriptorIndexMap.contains(id))
		return ms_descriptorIndexMap.at(id);

	UINT heapStart = static_cast<UINT>(ms_nextDescriptorIndex);
	ms_nextDescriptorIndex += sizes.size();

	if (sharing)
		ms_descriptorIndexMap.emplace(id, heapStart);

	for (size_t i = 0; i < sizes.size(); i++)
	{
		size_t alignedSize = (static_cast<size_t>(sizes[i]) + 255) & ~255; // Ceilings the size to the nearest 256

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = alignedSize;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* cbufferResource;
		d3d->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cbufferResource));
		cbvResources.push_back(cbufferResource);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = cbufferResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<UINT>(alignedSize);

		UINT incrementSize = d3d->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		auto cbvHandle = ms_srv_cbv_uav_Heap->GetCPUDescriptorHandleForHeapStart();
		cbvHandle.ptr += incrementSize * (heapStart + i);
		d3d->GetDevice()->CreateConstantBufferView(&cbvDesc, cbvHandle);

		ResourceManager::TransitionResource(commandListDirect, cbufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		if (ms_DebugHeapEnabled)
		{
			string shareStr = sharing ? "(PerFrame)" : "(PerDraw)";
			ms_debugHeapList.push_back("CBV: { size: " + std::to_string(sizes[i]) + " }, { Flags: " + shareStr + " }");
		}			
	}

	return heapStart;
}

void DescriptorManager::SetDynamicSRV(D3DClass* d3d, UINT index, DXGI_FORMAT format, ID3D12Resource* resource)
{
	if (!ms_initialised)
		throw std::exception("Uninitialised Descriptor Manager");

	auto device = d3d->GetDevice();

	UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = ms_srv_cbv_uav_Heap->GetCPUDescriptorHandleForHeapStart();

	// Assign SRV to material heap
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cpuHandle, index, incrementSize);
		device->CreateShaderResourceView(resource, &srvDesc, srvHandle);
	}
}

ID3D12DescriptorHeap* DescriptorManager::GetHeap()
{
	if (!ms_initialised)
		throw std::exception("Uninitialised Descriptor Manager");

	return ms_srv_cbv_uav_Heap.Get();
}

UINT DescriptorManager::GetIncrementSize()
{
	if (!ms_initialised)
		throw std::exception("Uninitialised Descriptor Manager");

	return ms_descriptorIncrementSize;
}

vector<string>& DescriptorManager::GetDebugHeapStrings()
{
	return ms_debugHeapList;
}

void DescriptorManager::Shutdown()
{
	ms_descriptorIndexMap.clear();
	ms_srv_cbv_uav_Heap.Reset();
}
