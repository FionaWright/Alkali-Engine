#pragma once

#include "pch.h"
#include "Texture.h"

using std::shared_ptr;

struct RootParamInfo;

class Material
{
public:
	Material();
	~Material();

	void AddSRVs(D3DClass* d3d, vector<Texture*> textures);	
	void AddDynamicSRVs(UINT count);	
	void AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes, bool perFrame);

	void SetCBV_PerFrame(UINT registerIndex, void* srcData, size_t dataSize);
	void SetCBV_PerDraw(UINT registerIndex, void* srcData, size_t dataSize);
	void SetDynamicSRV(D3DClass* d3d, UINT registerIndex, DXGI_FORMAT format, ID3D12Resource* resource);

	void AssignMaterial(ID3D12GraphicsCommandList2* commandList, RootParamInfo& rootParamInfo);

	bool GetHasAlpha();
	vector<Texture*>& GetTextures();

private:
	UINT m_srvHeapIndex = -1, m_cbvHeapIndex_perFrame = -1, m_cbvHeapIndex_perDraw = -1;
	vector<ID3D12Resource*> m_cbvResources_perFrame;		
	vector<ID3D12Resource*> m_cbvResources_perDraw;		
	vector<Texture*> m_textures;
};

