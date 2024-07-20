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
	static void Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, Frustum& frustum);
	static void Shutdown();

	static void Update(D3DClass* d3d, XMFLOAT3 lightDir, Frustum& frustum, const XMFLOAT3& eyePos);
	static void UpdateDebugLines(D3DClass* d3d);
	static void CalculateBounds(XMFLOAT3 lightDir, Frustum& frustum);
	static void Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, unordered_map<string, shared_ptr<Batch>>& batchList, Frustum& frustum);
	static void RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle);

	static void SetDebugLines(vector<DebugLine*>& debugLines);

	static ID3D12Resource* GetShadowMap();	
	static XMMATRIX* GetVPMatrices();	
	static XMFLOAT4 GetCascadeDistances();
	static float GetPCFSampleRange(int sampleCount);

private:
	static void CalculateSceneBounds(BoundsArgs args, float& width, float& height, float& nearDist, float& farDist);

	static CascadeInfo ms_cascadeInfos[MAX_SHADOW_MAP_CASCADES];

	static XMMATRIX ms_viewMatrix;
	static XMMATRIX ms_projMatrices[MAX_SHADOW_MAP_CASCADES];
	static XMMATRIX ms_vpMatrices[MAX_SHADOW_MAP_CASCADES];

	static XMFLOAT3 ms_maxBasis, ms_forwardBasis;

	static ComPtr<ID3D12DescriptorHeap> ms_dsvHeap;
	static UINT ms_dsvDescriptorSize;
	static ComPtr<ID3D12Resource> ms_shadowMapResource;

	static D3D12_RESOURCE_STATES ms_currentDSVState;

	static std::unique_ptr<GameObject> ms_viewGO;
	static shared_ptr<Shader> ms_depthShader;
	static shared_ptr<RootSig> ms_depthRootSig;
	static shared_ptr<Material> ms_depthMat, ms_viewDepthMat;
	static shared_ptr<RootSig> ms_viewRootSig;

	static D3D12_VIEWPORT ms_viewports[MAX_SHADOW_MAP_CASCADES];

	static vector<DebugLine*> ms_debugLines;

	static bool ms_initialised;
};

