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
	void AddDynamicSRVs(string id, UINT count);	
	void AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, const vector<UINT>& sizes, bool perFrame, string id = "");

	void SetCBV_PerFrame(UINT resourceIndex, void* srcData, size_t dataSize, const int& backBufferIndex);
	void SetCBV_PerFrame(UINT resourceIndex, void* srcData, size_t dataSize);
	void SetCBV_PerDraw(UINT resourceIndex, void* srcData, size_t dataSize, const int& backBufferIndex);
	void SetCBV_PerDraw(UINT resourceIndex, void* srcData, size_t dataSize);
	void SetDynamicSRV(D3DClass* d3d, UINT registerIndex, DXGI_FORMAT format, ID3D12Resource* resource) const;

	void AttachProperties(const MaterialPropertiesCB& matProp);
	void AttachThinFilm(const ThinFilmCB& thinFilm);

	void AssignMaterial(ID3D12GraphicsCommandList2* cmdList, const RootParamInfo& rootParamInfo, const int& backBufferIndex) const;

	bool GetHasAlpha();
	vector<shared_ptr<Texture>>& GetTextures();
	void GetIndices(UINT& srv, UINT& cbvFrame, UINT& cbvDraw, const int& backBufferIndex) const;
	bool GetProperties(MaterialPropertiesCB& prop) const;
	bool GetThinFilm(ThinFilmCB& thinFilm) const;
	bool HasDynamicSRV() const;
	bool IsLoaded() const;

	bool TryUploadSRVs(D3DClass* d3d);

	void ClearTextures();

private:
	bool IsTexturesLoaded();

	UINT m_srvHeapIndex = -1, m_srvHeapIndex_dynamic = -1, m_cbvHeapIndex_perFrame[BACK_BUFFER_COUNT], m_cbvHeapIndex_perDraw[BACK_BUFFER_COUNT];
	vector<ID3D12Resource*> m_cbvResources_perFrame[BACK_BUFFER_COUNT];		
	vector<ID3D12Resource*> m_cbvResources_perDraw[BACK_BUFFER_COUNT];
	vector<shared_ptr<Texture>> m_textures;

	size_t m_addedCBV_PerDraw = 0, m_addedCBV_PerFrame = 0, m_addedSRV = 0, m_addedSRV_dynamic = 0;

	MaterialPropertiesCB m_propertiesCB;
	bool m_attachedProperties = false;
	ThinFilmCB m_thinFilmCB;
	bool m_attachedThinFilm = false;
	
	bool m_texturesLoaded = false;
};

