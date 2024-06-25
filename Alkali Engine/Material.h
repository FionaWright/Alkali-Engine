#pragma once

#include "pch.h"
#include "Texture.h"

using std::shared_ptr;

class Material
{
public:
	Material();
	~Material();

	void AddSRVs(D3DClass* d3d, vector<Texture*> textures);	
	void AddDynamicSRVs(UINT count);	
	void AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes);

	void SetCBV(UINT registerIndex, void* data, size_t dataSize);
	void SetDynamicSRV(D3DClass* d3d, UINT registerIndex, DXGI_FORMAT format, ID3D12Resource* resource);

	void AssignMaterial(ID3D12GraphicsCommandList2* commandList);

	bool GetHasAlpha();
	vector<Texture*>& GetTextures();

private:
	UINT m_srvHeapIndex = -1, m_cbvHeapIndex = -1;
	vector<ID3D12Resource*> m_cbvResources;		
	vector<Texture*> m_textures;
};

