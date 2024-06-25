#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"
#include "DescriptorManager.h"

Material::Material()
{	
}

Material::~Material()
{
    for (int i = 0; i < m_cbvResources.size(); i++)
    {
        m_cbvResources[i]->Release();
    }
}

void Material::AddSRVs(D3DClass* d3d, vector<Texture*> textures)
{
    m_srvHeapIndex = DescriptorManager::AddSRVs(d3d, textures);
    m_textures = textures;
}

void Material::AddDynamicSRVs(UINT count)
{
    m_srvHeapIndex = DescriptorManager::AddDynamicSRVs(count);
}

void Material::AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes)
{
    m_cbvHeapIndex = DescriptorManager::AddCBVs(d3d, commandListDirect, sizes, m_cbvResources);
}

void Material::SetCBV(UINT registerIndex, void* srcData, size_t dataSize) 
{
    void* dstData = nullptr;
    D3D12_RANGE readRange = {};
    m_cbvResources.at(registerIndex)->Map(0, &readRange, &dstData);

    std::memcpy(dstData, srcData, dataSize);

    m_cbvResources.at(registerIndex)->Unmap(0, nullptr);
}

void Material::SetDynamicSRV(D3DClass* d3d, UINT registerIndex, DXGI_FORMAT format, ID3D12Resource* resource)
{
    DescriptorManager::SetDynamicSRV(d3d, m_srvHeapIndex + registerIndex, format, resource);
}

void Material::AssignMaterial(ID3D12GraphicsCommandList2* commandList)
{
    UINT incrementSize = DescriptorManager::GetIncrementSize();

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = DescriptorManager::GetHeap()->GetGPUDescriptorHandleForHeapStart();    

    if (m_cbvHeapIndex != -1)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(gpuHandle, m_cbvHeapIndex, incrementSize);
        commandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
    }

    if (m_srvHeapIndex != -1)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(gpuHandle, m_srvHeapIndex, incrementSize);
        commandList->SetGraphicsRootDescriptorTable(1, srvHandle);
    }    
}

bool Material::GetHasAlpha()
{
    return m_textures.at(0)->GetHasAlpha();
}

vector<Texture*>& Material::GetTextures()
{
	return m_textures;
}
