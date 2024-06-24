#pragma once

#include "pch.h"
#include "GameObject.h"
#include "Frustum.h"

#include <vector>

using std::vector;

class GameObject;

class Batch
{
public:
	void Init(string name, ComPtr<ID3D12RootSignature> pRootSig);
	void Init(string name, CD3DX12_ROOT_PARAMETER1* params, UINT paramCount);

	GameObject* AddGameObject(GameObject go);
	void Render(ID3D12GraphicsCommandList2* commandList, XMMATRIX& viewProj, Frustum& frustum, PerFrameCBuffers* perFrameCB);
	void RenderTrans(ID3D12GraphicsCommandList2* commandList, XMMATRIX& viewProj, Frustum& frustum, PerFrameCBuffers* perFrameCB);

	void SortObjects(const XMFLOAT3& camPos);

	vector<GameObject>& GetOpaques();
	vector<GameObject>& GetTrans();

	string m_Name;

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	vector<GameObject> m_goList;
	vector<GameObject> m_goListTrans;
	// Add option to sort by depth value as well
};

