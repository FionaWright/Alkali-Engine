#pragma once

#include "pch.h"
#include "GameObject.h"

#include <vector>

using std::vector;

class GameObject;

class Batch
{
public:
	void Init(ComPtr<ID3D12RootSignature> pRootSig);
	void AddGameObject(GameObject* go);
	void Render(ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj);

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	vector<GameObject*> m_gameObjectList; // Sorted by PipelineState then Model
	// Add option to sort by depth value as well
};

