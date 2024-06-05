#pragma once

#include "pch.h"
#include "Texture.h"

using std::shared_ptr;

class Material
{
public:
	Material(UINT numSamplers);
	~Material();

	void AddTexture(ID3D12Device2* device, shared_ptr<Texture> tex);

	ID3D12DescriptorHeap* GetTextureHeap();

private:
	vector<shared_ptr<Texture>> m_textures; // Not sure if I like this, it's just to keep them in memory
	ComPtr<ID3D12DescriptorHeap> m_textureHeap;
	int m_expectedTextureCount;
	int m_texturesAddedCount;
};

