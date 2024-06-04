#include "pch.h"
#include "Material.h"
#include "ResourceManager.h"

Material::Material(UINT numSamplers)
{
	m_samplerHeap = ResourceManager::CreateDescriptorHeap(numSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

Material::~Material()
{
}

void Material::AddTexture(ComPtr<ID3D12Device2> device, Texture* tex)
{
	tex->AddToDescriptorHeap(device, m_samplerHeap);
}

ComPtr<ID3D12DescriptorHeap> Material::GetSamplerHeap()
{
	return m_samplerHeap;
}
