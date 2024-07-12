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
	bool UseShadowMapMat = false;
	float FrustumNearPercent = 0.0f, FrustumFarPercent = 1.0f;
	int CascadeIndex = -1;
};

class Batch
{
public:
	Batch();
	~Batch();

	void Init(string name, shared_ptr<RootSig> pRootSig);

	GameObject* AddGameObject(GameObject go);
	GameObject* CreateGameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial = nullptr, bool orthoGraphic = false);

	void Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* override = nullptr);
	void RenderTrans(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* override = nullptr);

	void SortObjects(const XMFLOAT3& camPos);

	vector<GameObject>& GetOpaques();
	vector<GameObject>& GetTrans();

	string m_Name;

private:
	shared_ptr<RootSig> m_rootSig = nullptr;
	vector<GameObject> m_goList;
	vector<GameObject> m_goListTrans;
	// Add option to sort by depth value as well
};

