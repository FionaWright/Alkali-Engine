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

void Material::Init(UINT numTex)
{
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

ID3D12DescriptorHeap* Material::GetTextureHeap()
{
	return m_textureHeap.Get();
}

bool Material::GetHasAlpha()
{
	return m_textures.at(0)->GetHasAlpha();
}

vector<shared_ptr<Texture>>& Material::GetTextures()
{
	return m_textures;
}
