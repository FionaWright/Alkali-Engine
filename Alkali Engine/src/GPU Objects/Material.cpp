#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"
#include "DescriptorManager.h"
#include "Scene.h"

Material::Material()
{	
    for (int i = 0; i < BACK_BUFFER_COUNT; i++)
    {
        m_cbvHeapIndex_perFrame[i] = -1;
        m_cbvHeapIndex_perDraw[i] = -1;
    }
}

Material::~Material()
{
    for (int b = 0; b < BACK_BUFFER_COUNT; b++)
    {
        for (int i = 0; i < m_cbvResources_perFrame[b].size(); i++)
        {
            m_cbvResources_perFrame[b][i]->Release();
        }

        for (int i = 0; i < m_cbvResources_perDraw[b].size(); i++)
        {
            m_cbvResources_perDraw[b][i]->Release();
        }
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

void Material::AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, const vector<UINT>& sizes, bool perFrame, string id)
{
    if (perFrame)
    {
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            m_cbvHeapIndex_perFrame[i] = DescriptorManager::AddCBVs(d3d, cmdListDirect, sizes, m_cbvResources_perFrame[i], true, id + "{" + std::to_string(i) + "}");
        }        

        m_addedCBV_PerFrame += sizes.size();
        return;
    }        
    
    for (int i = 0; i < BACK_BUFFER_COUNT; i++)
    {
        m_cbvHeapIndex_perDraw[i] = DescriptorManager::AddCBVs(d3d, cmdListDirect, sizes, m_cbvResources_perDraw[i], false, id);
    }    
    m_addedCBV_PerDraw += sizes.size();
}

void Material::SetCBV_PerFrame(UINT resourceIndex, void* srcData, size_t dataSize, const int& backBufferIndex)
{
    if (m_cbvResources_perFrame[backBufferIndex].size() <= resourceIndex)
        throw std::exception("Material does not have ownership of that resource");

    void* dstData = nullptr;
    D3D12_RANGE readRange = {};
    m_cbvResources_perFrame[backBufferIndex].at(resourceIndex)->Map(0, &readRange, &dstData);

    std::memcpy(dstData, srcData, dataSize);

    m_cbvResources_perFrame[backBufferIndex].at(resourceIndex)->Unmap(0, nullptr);
}

void Material::SetCBV_PerDraw(UINT resourceIndex, void* srcData, size_t dataSize, const int& backBufferIndex)
{
    if (m_cbvResources_perDraw[backBufferIndex].size() <= resourceIndex)
        throw std::exception("Material does not have ownership of that resource");

    void* dstData = nullptr;
    D3D12_RANGE readRange = {};
    m_cbvResources_perDraw[backBufferIndex].at(resourceIndex)->Map(0, &readRange, &dstData);

    std::memcpy(dstData, srcData, dataSize);

    m_cbvResources_perDraw[backBufferIndex].at(resourceIndex)->Unmap(0, nullptr);
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

void Material::AssignMaterial(ID3D12GraphicsCommandList2* cmdList, const RootParamInfo& rootParamInfo, const int& backBufferIndex)
{
    if (rootParamInfo.NumCBV_PerDraw != m_addedCBV_PerDraw ||
        rootParamInfo.NumCBV_PerFrame != m_addedCBV_PerFrame ||
        rootParamInfo.NumSRV != m_addedSRV ||
        rootParamInfo.NumSRV_Dynamic != m_addedSRV_dynamic)
        throw std::exception("Invalid RootParamInfo based on created resources");

    UINT incrementSize = DescriptorManager::GetIncrementSize();

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = DescriptorManager::GetHeap()->GetGPUDescriptorHandleForHeapStart();    

    if (backBufferIndex >= 0 && m_cbvHeapIndex_perDraw[backBufferIndex] != -1 && rootParamInfo.NumCBV_PerDraw > 0 && rootParamInfo.ParamIndexCBV_PerDraw >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(gpuHandle, m_cbvHeapIndex_perDraw[backBufferIndex], incrementSize);
        cmdList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexCBV_PerDraw, cbvHandle);
    }

    if (backBufferIndex >= 0 && m_cbvHeapIndex_perFrame[backBufferIndex] != -1 && rootParamInfo.NumCBV_PerFrame > 0 && rootParamInfo.ParamIndexCBV_PerFrame >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(gpuHandle, m_cbvHeapIndex_perFrame[backBufferIndex], incrementSize);
        cmdList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexCBV_PerFrame, cbvHandle);
    }

    if (m_srvHeapIndex != -1 && rootParamInfo.NumSRV > 0 && rootParamInfo.ParamIndexSRV >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(gpuHandle, m_srvHeapIndex, incrementSize);
        cmdList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexSRV, srvHandle);
    }    

    if (m_srvHeapIndex_dynamic != -1 && rootParamInfo.NumSRV_Dynamic > 0 && rootParamInfo.ParamIndexSRV_Dynamic >= 0)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(gpuHandle, m_srvHeapIndex_dynamic, incrementSize);
        cmdList->SetGraphicsRootDescriptorTable(rootParamInfo.ParamIndexSRV_Dynamic, srvHandle);
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

void Material::GetIndices(UINT& srv, UINT& cbvFrame, UINT& cbvDraw, const int& backBufferIndex)
{
    srv = m_srvHeapIndex;
    cbvFrame = m_cbvHeapIndex_perFrame[backBufferIndex];
    cbvDraw = m_cbvHeapIndex_perDraw[backBufferIndex];
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
