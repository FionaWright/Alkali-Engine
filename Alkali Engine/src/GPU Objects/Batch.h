#pragma once

#include "pch.h"
#include "GameObject.h"
#include "Frustum.h"

#include <vector>
#include "RootSig.h"

using std::vector;

class GameObject;

struct RenderOverride
{
	Shader* ShaderOverride;
	RootSig* RootSigOverride;
	bool UseDepthMaterial = false;
	float FrustumNearPercent = 0.0f, FrustumFarPercent = 1.0f;
	int DepthMatIndex = -1;
	bool AddSRVToDepthMat = false;

	XMFLOAT3 Origin = XMFLOAT3_ZERO;
	XMFLOAT3 MaxBasis = XMFLOAT3_ZERO;
	XMFLOAT3 ForwardBasis = XMFLOAT3_ZERO;
	bool CullAgainstBounds = false;
	float BoundsWidth = 0, BoundsHeight = 0, BoundsNear = 0, BoundsFar = 0;
};

class Batch
{
public:
	Batch();
	~Batch();

	void Init(string name, shared_ptr<RootSig> pRootSig);

	GameObject* AddGameObject(GameObject go);
	GameObject* CreateGameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial = nullptr, bool orthoGraphic = false, bool forceTransparent = false);

	void Render(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, const int& backBufferIndex, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* override = nullptr, bool* requireCPUGPUSync = nullptr, Shader** lastSetShader = nullptr);
	void RenderTrans(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, const int& backBufferIndex, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* override = nullptr, bool* requireCPUGPUSync = nullptr, Shader** lastSetShader = nullptr);

	void SortObjects(const XMFLOAT3& camPos);

	vector<GameObject>& GetOpaques();
	vector<GameObject>& GetTrans();
	RootParamInfo& GetRoomParamInfo();

	string m_Name;

private:
	shared_ptr<RootSig> m_rootSig = nullptr;
	vector<GameObject> m_goList;
	vector<GameObject> m_goListTrans;
};

