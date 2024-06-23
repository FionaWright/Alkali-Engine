#include "pch.h"
#include "Batch.h"
#include "CBuffers.h"
#include "Utils.h"

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

GameObject* Batch::AddGameObject(GameObject go)
{
	if (go.IsTransparent())
	{
		m_goListTrans.push_back(go);		
		return &m_goListTrans[m_goListTrans.size() - 1];
	}

	// TODO: Sorting
	m_goList.push_back(go);
	return &m_goList[m_goList.size() - 1];
}

void Batch::Render(ID3D12GraphicsCommandList2* commandList, XMMATRIX& viewProj, Frustum& frustum)
{
	MatricesCB matrices;
	matrices.VP = viewProj;

	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (int i = 0; i < m_goList.size(); i++) 
	{
		XMFLOAT3 pos;
		float radius;
		m_goList[i].GetBoundingSphere(pos, radius);

		if (!FRUSTUM_CULLING_ENABLED || m_goList[i].IsOrthographic() || frustum.CheckSphere(pos, radius))
			m_goList[i].Render(commandList, &matrices);
	}
}

void Batch::RenderTrans(ID3D12GraphicsCommandList2* commandList, XMMATRIX& viewProj, Frustum& frustum)
{
	MatricesCB matrices;
	matrices.VP = viewProj;

	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (int i = 0; i < m_goListTrans.size(); i++)
	{
		XMFLOAT3 pos;
		float radius;
		m_goListTrans[i].GetBoundingSphere(pos, radius);

		if (!FRUSTUM_CULLING_ENABLED || m_goListTrans[i].IsOrthographic() || frustum.CheckSphere(pos, radius))
			m_goListTrans[i].Render(commandList, &matrices);
	}
}

void Batch::SortObjects(const XMFLOAT3& camPos) 
{
	std::sort(m_goList.begin(), m_goList.end(), [&camPos](const GameObject& a, const GameObject& b) {
		float distSqA = SqDist(a.GetWorldPosition(), camPos);
		float distSqB = SqDist(b.GetWorldPosition(), camPos);
		return distSqA > distSqB;
	});

	std::sort(m_goListTrans.begin(), m_goListTrans.end(), [&camPos](const GameObject& a, const GameObject& b) {
		float distSqA = SqDist(a.GetWorldPosition(), camPos);
		float distSqB = SqDist(b.GetWorldPosition(), camPos);
		return distSqA > distSqB;
	});
}

vector<GameObject>& Batch::GetOpaques()
{
	return m_goList;
}

vector<GameObject>& Batch::GetTrans()
{
	return m_goListTrans;
}
