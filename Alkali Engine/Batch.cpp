#include "pch.h"
#include "Batch.h"

Batch::Batch(ComPtr<ID3D12RootSignature> pRootSig)
	:
	m_rootSignature(pRootSig)
{
}

Batch::Batch(CD3DX12_ROOT_PARAMETER1* params, UINT paramCount)
	:
	m_rootSignature(ResourceManager::CreateRootSignature(params, paramCount, nullptr, 0))
{
}

void Batch::AddGameObject(shared_ptr<GameObject> go, bool transparent)
{
	if (transparent)
	{
		m_gameObjectListTransparent.push_back(go);
		return;
	}

	// TODO: Sorting
	m_gameObjectList.push_back(go);
}

void Batch::Render(ID3D12GraphicsCommandList2* commandList, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj)
{
	for (int i = 0; i < m_gameObjectList.size(); i++) 
	{
		m_gameObjectList.at(i)->Render(commandList, m_rootSignature.Get(), viewPort, scissorRect, rtv, dsv, viewProj);
	}

	for (int i = 0; i < m_gameObjectListTransparent.size(); i++)
	{
		m_gameObjectListTransparent.at(i)->Render(commandList, m_rootSignature.Get(), viewPort, scissorRect, rtv, dsv, viewProj);
	}
}

void Batch::AddHeldGameObjectsToList(vector<GameObject*>& list) 
{
	for (size_t i = 0; i < m_gameObjectList.size(); i++)
	{
		list.push_back(m_gameObjectList.at(i).get());
	}

	for (size_t i = 0; i < m_gameObjectListTransparent.size(); i++)
	{
		list.push_back(m_gameObjectListTransparent.at(i).get());
	}
}