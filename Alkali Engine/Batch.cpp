#include "pch.h"
#include "Batch.h"
#include "CBuffers.h"
#include "Utils.h"

Batch::Batch()
	: m_Name("Uninitialised")
{
}

Batch::~Batch()
{
}

void Batch::Init(string name, shared_ptr<RootSig> pRootSig)
{
	m_Name = name;
	m_rootSig = pRootSig;
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

GameObject* Batch::CreateGameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial, bool orthoGraphic)
{
	GameObject go(name, pModel, pShader, pMaterial, orthoGraphic);
	return AddGameObject(go);
}

bool CheckWithinBounds(RenderOverride* ro, XMFLOAT3 pos, float radius)
{
	XMFLOAT3 forward = Mult(pos, ro->ForwardBasis);
	if (forward.z - radius < ro->BoundsNear || forward.z + radius > ro->BoundsFar)
		return false;

	XMFLOAT3 pMin = Add(Abs(pos), Mult(ro->MaxBasis, -radius));

	if (pMin.x * 2 <= ro->BoundsWidth || pMin.y * 2 <= ro->BoundsHeight)
		return true;

	return false;
}

void RenderFromList(D3DClass* d3d, vector<GameObject>& list, RootSig* rootSig, ID3D12GraphicsCommandList2* commandList, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* renderOverride)
{
	MatricesCB matrices;
	matrices.V = view;
	matrices.P = proj;

	commandList->SetGraphicsRootSignature(rootSig->GetRootSigResource());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (int i = 0; i < list.size(); i++)
	{
		XMFLOAT3 pos;
		float radius;
		list[i].GetBoundingSphere(pos, radius);

		if (renderOverride && renderOverride->CullAgainstBounds && !CheckWithinBounds(renderOverride, pos, radius))
			continue;

		float frustumNear = renderOverride ? renderOverride->FrustumNearPercent : 0.0f;
		float frustumFar = renderOverride ? renderOverride->FrustumFarPercent: 1.0f;

		if (!SettingsManager::ms_Dynamic.FrustumCullingEnabled || list[i].IsOrthographic() || !frustum || frustum->CheckSphere(pos, radius, frustumNear, frustumFar))
			list[i].Render(d3d, commandList, rootSig->GetRootParamInfo(), &matrices, renderOverride);
	}
}

void Batch::Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* renderOverride)
{
	RootSig* rootSig = renderOverride && renderOverride->RootSigOverride ? renderOverride->RootSigOverride : m_rootSig.get();
	RenderFromList(d3d, m_goList, rootSig, commandList, view, proj, frustum, renderOverride);
}

void Batch::RenderTrans(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, XMMATRIX& view, XMMATRIX& proj, Frustum* frustum, RenderOverride* renderOverride)
{
	RootSig* rootSig = renderOverride && renderOverride->RootSigOverride ? renderOverride->RootSigOverride : m_rootSig.get();
	RenderFromList(d3d, m_goListTrans, rootSig, commandList, view, proj, frustum, renderOverride);
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
