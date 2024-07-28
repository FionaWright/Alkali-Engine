#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"
#include "DescriptorManager.h"
#include "Scene.h"

Material::Material()
{	
}

Material::~Material()
{
    for (int i = 0; i < m_cbvResources_perFrame.size(); i++)
    {
        m_cbvResources_perFrame[i]->Release();
    }

    for (int i = 0; i < m_cbvResources_perDraw.size(); i++)
    {
        m_cbvResources_perDraw[i]->Release();
    }
}

void Material::AddSRVs(D3DClass* d3d, vector<shared_ptr<Texture>> textures)
{
    m_srvHeapIndex = DescriptorManager::AddSRVs(d3d, textures);   
    m_textures = textures;
    m_addedSRV += textures.size();
}

void Material::AddDynamicSRVs(string id, UINT count)
{
    m_srvHeapIndex_dynamic = DescriptorManager::AddDynamicSRVs(id, count);
    m_addedSRV_dynamic += count;
}

void Material::AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes, bool perFrame, string id)
{
    if (perFrame)
    {
        m_cbvHeapIndex_perFrame = DescriptorManager::AddCBVs(d3d, commandListDirect, sizes, m_cbvResources_perFrame, true, id);
        m_addedCBV_PerFrame += sizes.size();
        return;
    }        
    
    m_cbvHeapIndex_perDraw = DescriptorManager::AddCBVs(d3d, commandListDirect, sizes, m_cbvResources_perDraw, false, id);
    m_addedCBV_PerDraw += sizes.size();
}

void Material::SetCBV_PerFrame(UINT resourceIndex, void* srcData, size_t dataSize) 
{
    if (m_cbvResources_perFrame.size() <= resourceIndex)
        throw std::exception("Material does not have ownership of that resource");

    void* dstData = nullptr;
    D3D12_RANGE readRange = {};
    m_cbvResources_perFrame.at(resourceIndex)->Map(0, &readRange, &dstData);

    std::memcpy(dstData, srcData, dataSize);

    m_cbvResources_perFrame.at(resourceIndex)->Unmap(0, nullptr);
}

void Material::SetCBV_PerDraw(UINT resourceIndex, void* srcData, size_t dataSize)
{
    if (m_cbvResources_perDraw.size() <= resourceIndex)
        throw std::exception("Material does not have ownership of that resource");

    void* dstData = nullptr;
    D3D12_RANGE readRange = {};
    m_cbvResources_perDraw.at(resourceIndex)->Map(0, &readRange, &dstData);

    std::memcpy(dstData, srcData, dataSize);

    m_cbvResources_perDraw.at(resourceIndex)->Unmap(0, nullptr);
}

void Material::SetDynamicSRV(D3DClass* d3d, UINT registerIndex, DXGI_FORMAT format, ID3D12Resource* resource)
{
    if (registerIndex >= m_addedSRV_dynamic)
        throw std::exception("Invalid register index");

    DescriptorManager::AssignDynamicSRV(d3d, m_srvHeapIndex_dynamic + registerIndex, format, resource);
}

void Material::AttachProperties(const MaterialPropertiesCB& matProp)
{
    m_propertiesCB = matProp;
    m_attachedProperties = true;
}

void Material::AssignMaterial(ID3D12GraphicsCommandList2* commandList, const RootParamInfo& rootParamInfo)
{
    if (rootParamInfo.NumCBV_PerDraw != m_addedCBV_PerDraw ||
        rootParamInfo.NumCBV_PerFrame != m_addedCBV_PerFrame ||
        rootParamInfo.NumSRV != m_addedSRV ||
        rootParamInfo.NumSRV_Dynamic != m_addedSRV_dynamic)
        throw std::exception("Invalid RootParamInfo based on created resources");

    UINT incrementSize = DescriptorManager::GetIncrementSize();

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = DescriptorManager::GetHeap()->GetGPUDescriptorHandleForHeapStart();    

    if (m_cbvHeapIndex_perDraw != -1 && rootParamInfo.NumCBV_PerDraw > 0 && rootParamInfo.ParamIndexCBV_PerDraw >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(gpuHandle, m_cbvHeapIndex_perDraw, incrementSize);
        commandList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexCBV_PerDraw, cbvHandle);
    }

    if (m_cbvHeapIndex_perFrame != -1 && rootParamInfo.NumCBV_PerFrame > 0 && rootParamInfo.ParamIndexCBV_PerFrame >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(gpuHandle, m_cbvHeapIndex_perFrame, incrementSize);
        commandList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexCBV_PerFrame, cbvHandle);
    }

    if (m_srvHeapIndex != -1 && rootParamInfo.NumSRV > 0 && rootParamInfo.ParamIndexSRV >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(gpuHandle, m_srvHeapIndex, incrementSize);
        commandList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexSRV, srvHandle);
    }    

    if (m_srvHeapIndex_dynamic != -1 && rootParamInfo.NumSRV_Dynamic > 0 && rootParamInfo.ParamIndexSRV_Dynamic >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(gpuHandle, m_srvHeapIndex_dynamic, incrementSize);
        commandList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexSRV_Dynamic, srvHandle);
    }
}

bool Material::GetHasAlpha()
{
    if (m_textures.size() == 0)
        return false;

    return m_textures.at(0)->GetHasAlpha();
}

vector<shared_ptr<Texture>>& Material::GetTextures()
{
	return m_textures;
}

void Material::GetIndices(UINT& srv, UINT& cbvFrame, UINT& cbvDraw)
{
    srv = m_srvHeapIndex;
    cbvFrame = m_cbvHeapIndex_perFrame;
    cbvDraw = m_cbvHeapIndex_perDraw;
}

bool Material::GetProperties(MaterialPropertiesCB& prop)
{
    if (!m_attachedProperties)
        return false;

    prop = m_propertiesCB;
    return true;
}

bool Material::HasDynamicSRV()
{
    return m_addedSRV_dynamic > 0;
}

void Material::ClearTextures()
{
    m_textures.clear();
    m_addedSRV = 0;
}
