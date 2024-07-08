#pragma once

#include "pch.h"
#include "GameObject.h"
#include "Frustum.h"

#include <vector>
#include "RootSig.h"

using std::vector;

class GameObject;

class Batch
{
public:
	Batch();
	~Batch();

	void Init(string name, shared_ptr<RootSig> pRootSig);

	GameObject* AddGameObject(GameObject go);
	GameObject* CreateGameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial = nullptr, bool orthoGraphic = false);
	void Render(ID3D12GraphicsCommandList2* commandList, XMMATRIX& viewProj, Frustum& frustum);
	void RenderTrans(ID3D12GraphicsCommandList2* commandList, XMMATRIX& viewProj, Frustum& frustum);

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

