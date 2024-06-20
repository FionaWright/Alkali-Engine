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

	void AddGameObject(shared_ptr<GameObject> go);
	void Render(ID3D12GraphicsCommandList2* commandList, D3D12_VIEWPORT& viewPort, D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE& rtv, D3D12_CPU_DESCRIPTOR_HANDLE& dsv, XMMATRIX& viewProj, Frustum& frustum);

	vector<shared_ptr<GameObject>> GetOpaques();
	vector<shared_ptr<GameObject>> GetTrans();

	string m_Name;

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	vector<shared_ptr<GameObject>> m_gameObjectList; // Sorted by PipelineState then Model
	vector<shared_ptr<GameObject>> m_gameObjectListTransparent; // Sorted by PipelineState then Model
	// Add option to sort by depth value as well
};

