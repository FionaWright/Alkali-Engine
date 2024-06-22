#pragma once

#include "pch.h"
#include "Texture.h"

using std::shared_ptr;

class Material
{
public:
	Material();
	~Material();

	void Init(UINT numSamplers, UINT rootParamIndex);

	void AddTexture(D3DClass* d3d, shared_ptr<Texture> tex);

	void AssignTextureResourceManually(D3DClass* d3d, int offset, DXGI_FORMAT format, ID3D12Resource* resource);

	ID3D12DescriptorHeap* GetTextureHeap();

	bool GetHasAlpha();
	UINT GetRootParamIndex();

	vector<shared_ptr<Texture>>& GetTextures();

private:
	vector<shared_ptr<Texture>> m_textures; // Not sure if I like this, it's just to keep them in memory. And now also for ImGui stuff
	ComPtr<ID3D12DescriptorHeap> m_textureHeap;
	int m_expectedTextureCount;
	int m_texturesAddedCount;
	UINT m_rootParamIndex = -1;
};

