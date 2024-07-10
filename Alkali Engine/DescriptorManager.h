#pragma once

#include "pch.h"
#include <unordered_map>
#include <vector>

#include "Texture.h"
#include "Material.h"
#include "ResourceManager.h"

using std::string;
using std::unordered_map;
using std::vector;

class DescriptorManager
{
public:
	static void Init(D3DClass* d3d, UINT numDescriptors);
	static UINT AddSRVs(D3DClass* d3d, const vector<shared_ptr<Texture>>& textures);
	static UINT AddDynamicSRVs(string id, UINT count);
	static UINT AddCBVs(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, const vector<UINT>& sizes, vector<ID3D12Resource*>& cbvResources, bool sharing, string id = "");

	static void SetDynamicSRV(D3DClass* d3d, UINT index, DXGI_FORMAT format, ID3D12Resource* resource);

	static ID3D12DescriptorHeap* GetHeap();
	static UINT GetIncrementSize();
	static vector<string>& GetDebugHeapStrings();

	static void Shutdown();

private:
	static unordered_map<string, UINT> ms_descriptorIndexMap;
	static ComPtr<ID3D12DescriptorHeap> ms_srv_cbv_uav_Heap;
	static size_t ms_nextDescriptorIndex;
	static UINT ms_descriptorIncrementSize;

	static vector<string> ms_debugHeapList;

	static bool ms_initialised;
};

