#pragma once

#include "pch.h"
#include "Texture.h"

class Material
{
public:
	Material(UINT numSamplers);
	~Material();

	void AddTexture(ComPtr<ID3D12Device2> device, Texture* tex);

	ComPtr<ID3D12DescriptorHeap> GetSamplerHeap();
private:
	ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
};

