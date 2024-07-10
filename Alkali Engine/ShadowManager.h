#pragma once

#include "pch.h"
#include <vector>
#include <unordered_map>

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

class ShadowManager
{
public:
	static void Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList);
	static void Shutdown();

	static void Update(XMFLOAT3 lightDir);
	static void Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, unordered_map<string, shared_ptr<Batch>>& batchList);
	static void RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList);

	static ID3D12Resource* GetShadowMap();	
	static XMMATRIX& GetVPMatrix();

private:
	static XMMATRIX ms_viewMatrix, ms_projMatrix, ms_vpMatrix;

	static ComPtr<ID3D12DescriptorHeap> ms_dsvHeap;
	static UINT ms_dsvDescriptorSize;
	static ComPtr<ID3D12Resource> ms_shadowMapResource;

	static D3D12_RESOURCE_STATES ms_currentDSVState;

	static std::unique_ptr<GameObject> ms_viewGO;
	static shared_ptr<Shader> ms_depthShader;
	static shared_ptr<RootSig> ms_depthRootSig;
	static shared_ptr<Material> ms_depthMat;
};

