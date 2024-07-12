#pragma once

#include "pch.h"
#include <vector>
#include <unordered_map>
#include "SettingsManager.h"
#include "DebugLine.h"

class Shader;
class Material;
class GameObject;
class Batch;
class Frustum;
class D3DClass;
class RootSig;

using std::vector;
using std::shared_ptr;
using std::unordered_map;
using std::string;

struct CascadeInfo
{
	float NearPercent, FarPercent;

	float Width, Height, Near, Far;
};

struct BoundsArgs
{
	XMFLOAT3& forwardBasis, maxBasis;
	unordered_map<string, shared_ptr<Batch>>& batchList;
	Frustum& frustum;
	float cascadeNear = 0, cascadeFar = 1;
};

class ShadowManager
{
public:
	static void Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList);
	static void Shutdown();

	static void Update(D3DClass* d3d, XMFLOAT3 lightDir, Frustum& frustum);
	static void Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, unordered_map<string, shared_ptr<Batch>>& batchList, Frustum& frustum);
	static void RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle);

	static void SetDebugLines(vector<DebugLine*>& debugLines);

	static ID3D12Resource* GetShadowMap();	
	static XMMATRIX* GetVPMatrices();	
	static XMFLOAT4 GetCascadeDistances(float nearFarDist);

private:
	static void CalculateBounds(BoundsArgs args, float& width, float& height, float& nearDist, float& farDist);

	static CascadeInfo ms_cascadeInfos[SHADOW_MAP_CASCADES];

	static XMMATRIX ms_viewMatrix;
	static XMMATRIX ms_vpMatrices[SHADOW_MAP_CASCADES];

	static ComPtr<ID3D12DescriptorHeap> ms_dsvHeap;
	static UINT ms_dsvDescriptorSize;
	static ComPtr<ID3D12Resource> ms_shadowMapResource;

	static D3D12_RESOURCE_STATES ms_currentDSVState;

	static std::unique_ptr<GameObject> ms_viewGO;
	static shared_ptr<Shader> ms_depthShader;
	static shared_ptr<RootSig> ms_depthRootSig;
	static shared_ptr<Material> ms_depthMat, ms_viewDepthMat;
	static shared_ptr<RootSig> ms_viewRootSig;

	static D3D12_VIEWPORT ms_viewports[SHADOW_MAP_CASCADES];

	static vector<DebugLine*> ms_debugLines;
};

