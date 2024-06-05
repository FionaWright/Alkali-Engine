#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"

Material::Material(UINT numTextures)
	: m_texturesAddedCount(0)
	, m_expectedTextureCount(numTextures)
{
	m_textureHeap = ResourceManager::CreateDescriptorHeap(numTextures, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

Material::~Material()
{
}

void Material::AddTexture(ComPtr<ID3D12Device2> device, Texture* tex)
{
	if (m_texturesAddedCount >= m_expectedTextureCount)
		throw new std::exception("Too many textures being added to material");

	tex->AddToDescriptorHeap(device, m_textureHeap, m_texturesAddedCount);
	m_texturesAddedCount++;
}

ComPtr<ID3D12DescriptorHeap> Material::GetTextureHeap()
{
	return m_textureHeap;
}
