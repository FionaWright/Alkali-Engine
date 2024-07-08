#pragma once

#include "pch.h"
#include "Texture.h"
#include "CBuffers.h"

using std::shared_ptr;

struct RootParamInfo;

class Material
{
public:
	Material();
	~Material();

	void AddSRVs(D3DClass* d3d, vector<shared_ptr<Texture>> textures);
	void AddDynamicSRVs(UINT count);	
	void AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes, bool perFrame);

	void SetCBV_PerFrame(UINT resourceIndex, void* srcData, size_t dataSize);
	void SetCBV_PerDraw(UINT resourceIndex, void* srcData, size_t dataSize);
	void SetDynamicSRV(D3DClass* d3d, UINT registerIndex, DXGI_FORMAT format, ID3D12Resource* resource);

	void AttachProperties(const MaterialPropertiesCB& matProp);

	void AssignMaterial(ID3D12GraphicsCommandList2* commandList, const RootParamInfo& rootParamInfo);

	bool GetHasAlpha();
	vector<shared_ptr<Texture>>& GetTextures();
	void GetIndices(UINT& srv, UINT& cbvFrame, UINT& cbvDraw);
	bool GetProperties(MaterialPropertiesCB& prop);

	void ClearTextures();

private:
	UINT m_srvHeapIndex = -1, m_cbvHeapIndex_perFrame = -1, m_cbvHeapIndex_perDraw = -1;
	vector<ID3D12Resource*> m_cbvResources_perFrame;		
	vector<ID3D12Resource*> m_cbvResources_perDraw;		
	vector<shared_ptr<Texture>> m_textures;

	size_t m_addedCBV_PerDraw = 0, m_addedCBV_PerFrame = 0;

	MaterialPropertiesCB m_propertiesCB;
	bool m_attachedProperties;
};

