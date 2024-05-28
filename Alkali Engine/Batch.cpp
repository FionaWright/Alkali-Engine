#include "pch.h"
#include "Batch.h"

void Batch::Init(ComPtr<ID3D12RootSignature> pRootSig)
{
	m_rootSignature = pRootSig;
}

void Batch::AddGameObject(GameObject* go)
{
	// TODO: Sorting
	m_gameObjectList.push_back(go);
}

void Batch::Render(ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj)
{
	for (int i = 0; i < m_gameObjectList.size(); i++) 
	{
		m_gameObjectList.at(i)->Render(commandList, m_rootSignature, viewPort, scissorRect, rtv, dsv, viewProj);
	}
}
