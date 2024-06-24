#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"

Material::Material()
	: m_nextDescriptorIndex(0)
{	
}

Material::~Material()
{
    for (int i = 0; i < m_cbvResources.size(); i++)
    {
        m_cbvResources[i]->Release();
    }
}

void Material::Init(UINT numSRV, UINT numCBV)
{
    m_expectedDescriptors = numSRV + numCBV;
	m_srv_cbv_uav_Heap = ResourceManager::CreateDescriptorHeap(numSRV + numCBV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

void Material::AddTexture(D3DClass* d3d, shared_ptr<Texture> tex)
{
	if (m_nextDescriptorIndex >= m_expectedDescriptors)
		throw new std::exception("Too many descriptors being added to material");

	tex->AddToDescriptorHeap(d3d, m_srv_cbv_uav_Heap.Get(), m_nextDescriptorIndex);
	m_nextDescriptorIndex++;

	m_textures.push_back(tex);
}

// Used for assigning the depth buffer to a material for the visualisation
void Material::AssignTextureResourceManually(D3DClass* d3d, int offset, DXGI_FORMAT format, ID3D12Resource* resource)
{
    auto device = d3d->GetDevice();

    UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srv_cbv_uav_Heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srv_cbv_uav_Heap->GetGPUDescriptorHandleForHeapStart();

    // Assign SRV to material heap
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cpuHandle, offset, incrementSize);
        device->CreateShaderResourceView(resource, &srvDesc, srvHandle);
    }

    if (offset == m_nextDescriptorIndex)
        m_nextDescriptorIndex++;
}

ID3D12DescriptorHeap* Material::Get_SRV_CBV_UAV_Heap()
{
    if (m_nextDescriptorIndex != m_expectedDescriptors)
        throw new std::exception("Descriptors not fully assigned properly");

	return m_srv_cbv_uav_Heap.Get();
}

bool Material::GetHasAlpha()
{
	return m_textures.at(0)->GetHasAlpha();
}

void Material::AddCBuffer(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, size_t sizeOfCBuffer)
{
    if (m_nextDescriptorIndex >= m_expectedDescriptors)
        throw new std::exception("Too many descriptors being added to material");

    size_t alignedSize = (sizeOfCBuffer + 255) & ~255; // Ceilings the size to the nearest 256

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
    m_cbvResources.push_back(cbufferResource);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = cbufferResource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = alignedSize;

    UINT incrementSize = d3d->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto cbvHandle = m_srv_cbv_uav_Heap->GetCPUDescriptorHandleForHeapStart();    
    cbvHandle.ptr += incrementSize * m_nextDescriptorIndex; 
    d3d->GetDevice()->CreateConstantBufferView(&cbvDesc, cbvHandle);

    m_nextDescriptorIndex++;

    ResourceManager::TransitionResource(commandListDirect, cbufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void Material::SetCBuffer(UINT descriptorIndex, void* srcData, size_t dataSize) 
{
    void* dstData = nullptr;
    D3D12_RANGE readRange = {};
    m_cbvResources.at(descriptorIndex)->Map(0, &readRange, &dstData);

    std::memcpy(dstData, srcData, dataSize);

    m_cbvResources.at(descriptorIndex)->Unmap(0, nullptr);
}

vector<shared_ptr<Texture>>& Material::GetTextures()
{
	return m_textures;
}
