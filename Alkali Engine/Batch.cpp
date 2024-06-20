#include "pch.h"
#include "Batch.h"

void Batch::Init(string name, ComPtr<ID3D12RootSignature> pRootSig)
{
	m_Name = name;
	m_rootSignature = pRootSig;
}

void Batch::Init(string name, CD3DX12_ROOT_PARAMETER1* params, UINT paramCount)
{
	m_Name = name;
	m_rootSignature = ResourceManager::CreateRootSignature(params, paramCount, nullptr, 0);
}

void Batch::AddGameObject(shared_ptr<GameObject> go)
{
	if (go->IsTransparent())
	{
		m_gameObjectListTransparent.push_back(go);
		return;
	}

	// TODO: Sorting
	m_gameObjectList.push_back(go);
}

void Batch::Render(ID3D12GraphicsCommandList2* commandList, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj, Frustum& frustum)
{
	for (int i = 0; i < m_gameObjectList.size(); i++) 
	{
		XMFLOAT3 pos;
		float radius;
		m_gameObjectList.at(i)->GetBoundingSphere(pos, radius);

		if (!FRUSTUM_CULLING_ENABLED || frustum.CheckSphere(pos, radius))
			m_gameObjectList.at(i)->Render(commandList, m_rootSignature.Get(), viewPort, scissorRect, rtv, dsv, viewProj);
	}

	for (int i = 0; i < m_gameObjectListTransparent.size(); i++)
	{
		XMFLOAT3 pos;
		float radius;
		m_gameObjectListTransparent.at(i)->GetBoundingSphere(pos, radius);

		if (!FRUSTUM_CULLING_ENABLED || frustum.CheckSphere(pos, radius))
			m_gameObjectListTransparent.at(i)->Render(commandList, m_rootSignature.Get(), viewPort, scissorRect, rtv, dsv, viewProj);
	}
}

vector<shared_ptr<GameObject>> Batch::GetOpaques()
{
	return m_gameObjectList;
}

vector<shared_ptr<GameObject>> Batch::GetTrans()
{
	return m_gameObjectListTransparent;
}
