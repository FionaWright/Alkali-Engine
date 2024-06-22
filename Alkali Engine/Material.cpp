#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"

Material::Material()
	: m_texturesAddedCount(0)
	, m_expectedTextureCount(0)
{	
}

Material::~Material()
{
}

void Material::Init(UINT numTex, UINT rootParamIndex)
{
    m_rootParamIndex = rootParamIndex;

	m_expectedTextureCount = numTex;
	m_textureHeap = ResourceManager::CreateDescriptorHeap(m_expectedTextureCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

void Material::AddTexture(D3DClass* d3d, shared_ptr<Texture> tex)
{
	if (m_texturesAddedCount >= m_expectedTextureCount)
		throw new std::exception("Too many textures being added to material");

	tex->AddToDescriptorHeap(d3d, m_textureHeap.Get(), m_texturesAddedCount);
	m_texturesAddedCount++;

	m_textures.push_back(tex);
}

// Used for assigning the depth buffer to a material for the visualisation
void Material::AssignTextureResourceManually(D3DClass* d3d, int offset, DXGI_FORMAT format, ID3D12Resource* resource)
{
    auto device = d3d->GetDevice();

    UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_textureHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_textureHeap->GetGPUDescriptorHandleForHeapStart();

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
}

ID3D12DescriptorHeap* Material::GetTextureHeap()
{
	return m_textureHeap.Get();
}

bool Material::GetHasAlpha()
{
	return m_textures.at(0)->GetHasAlpha();
}

UINT Material::GetRootParamIndex()
{
    return m_rootParamIndex;
}

vector<shared_ptr<Texture>>& Material::GetTextures()
{
	return m_textures;
}
