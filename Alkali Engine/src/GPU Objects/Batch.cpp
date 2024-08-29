#include "pch.h"
#include "Batch.h"
#include "CBuffers.h"
#include "Utils.h"
#include "ShadowManager.h"

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
	if (go.IsAT())
	{
		m_goListAT.push_back(go);
		return &m_goListAT[m_goListAT.size() - 1];
	}

	if (go.IsTransparent())
	{
		m_goListTrans.push_back(go);		
		return &m_goListTrans[m_goListTrans.size() - 1];
	}

	m_goList.push_back(go);
	return &m_goList[m_goList.size() - 1];
}

GameObject* Batch::CreateGameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial, bool orthoGraphic, bool forceAT, bool forceTransparent)
{
	GameObject go(name, pModel, pShader, pMaterial, orthoGraphic);
	if (forceTransparent)
		go.ForceSetTransparent(true);
	else if (forceAT)
		go.ForceSetAT(true);

	return AddGameObject(go);
}

bool CheckWithinBounds(RenderOverride* ro, XMFLOAT3 pos, float radius, int index)
{
	pos = Subtract(pos, ro->Origin);

	XMFLOAT3 forward = Mult(pos, ro->ForwardBasis);
	if (forward.z - radius < ro->CullBoundsInfo[index].Near || forward.z + radius > ro->CullBoundsInfo[index].Far)
		return false;

	XMFLOAT3 pMin = Add(Abs(pos), Mult(ro->MaxBasis, -radius));

	if (pMin.x * 2 <= ro->CullBoundsInfo[index].Width || pMin.y * 2 <= ro->CullBoundsInfo[index].Height) // This might be too lenient, should be && right? Doesn't seem to work
		return true;

	return false;
}

void RenderFromList(D3DClass* d3d, vector<GameObject>& list, RootSig* rootSig, ID3D12GraphicsCommandList2* cmdList, BatchArgs& args)
{
	MatricesCB matrices;
	matrices.V = args.ViewMatrix;
	matrices.P = args.ProjMatrix;

	cmdList->SetGraphicsRootSignature(rootSig->GetRootSigResource());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	RootSig* lastSetRootSig = nullptr;	

	Shader* lastSetShaderPtr = nullptr;
	if (!args.ppLastUsedShader)
		args.ppLastUsedShader = &lastSetShaderPtr;

	bool frustumCullingOn = SettingsManager::ms_Dynamic.FrustumCullingEnabled && args.pFrustum;

	for (int i = 0; i < list.size(); i++)
	{
		XMFLOAT3 pos;
		float radius;
		list[i].GetBoundingSphere(pos, radius);

		if (args.pOverride && args.pOverride->CullBoundsInfo && args.pOverride->CullBoundsIndex != -1 && !CheckWithinBounds(args.pOverride, pos, radius, args.pOverride->CullBoundsIndex))
			continue;

		float frustumNear = args.pOverride ? args.pOverride->FrustumNearPercent : 0.0f;
		float frustumFar = args.pOverride ? args.pOverride->FrustumFarPercent: 1.0f;

		bool failedFrustumCull = frustumCullingOn && !args.pFrustum->CheckSphere(pos, radius, frustumNear, frustumFar);
		if (failedFrustumCull && !list[i].IsOrthographic())
			continue;

		UINT cullFlags = 0;
		if (args.pOverride && args.pOverride->CullBoundsInfo && args.pOverride->IsDepthMultiViewport)
		{
			for (int v = 0; v < SettingsManager::ms_Dynamic.Shadow.CascadeCount; v++)
			{
				if (CheckWithinBounds(args.pOverride, pos, radius, v))
					cullFlags |= 1 << v;
			}
		}

		list[i].Render(d3d, cmdList, rootSig->GetRootParamInfo(), args.BackBufferIndex, args.pRequireCPUGPUSync, &matrices, args.pOverride, args.ppLastUsedShader, cullFlags);
	}
}

void Batch::Render(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, BatchArgs& args)
{
	RootSig* rootSig = args.pOverride && args.pOverride->RootSigOverride ? args.pOverride->RootSigOverride : m_rootSig.get();
	RenderFromList(d3d, m_goList, rootSig, cmdList, args);
}

void Batch::RenderAT(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, BatchArgs& args)
{
	if (!SettingsManager::ms_Dynamic.ATGoEnabled)
		return;

	RootSig* rootSig = args.pOverride && args.pOverride->RootSigOverride ? args.pOverride->RootSigOverride : m_rootSig.get();
	RenderFromList(d3d, m_goListAT, rootSig, cmdList, args);
}

void Batch::RenderTrans(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, BatchArgs& args)
{
	if (!SettingsManager::ms_Dynamic.TransparentGOEnabled)
		return;

	RootSig* rootSig = args.pOverride && args.pOverride->RootSigOverride ? args.pOverride->RootSigOverride : m_rootSig.get();
	RenderFromList(d3d, m_goListTrans, rootSig, cmdList, args);
}

void Batch::SortObjects(const XMFLOAT3& camPos) 
{
	auto sortNearToFar = [&camPos](const GameObject& a, const GameObject& b) {
		float distSqA = SqDist(a.GetWorldPosition(), camPos);
		float distSqB = SqDist(b.GetWorldPosition(), camPos);
		return distSqA < distSqB;
	};

	auto sortFarToNear = [&camPos](const GameObject& a, const GameObject& b) {
		float distSqA = SqDist(a.GetWorldPosition(), camPos);
		float distSqB = SqDist(b.GetWorldPosition(), camPos);
		return distSqA > distSqB;
	};

	std::sort(m_goList.begin(), m_goList.end(), sortNearToFar);

	std::sort(m_goListTrans.begin(), m_goListTrans.end(), sortFarToNear);

	if (SettingsManager::ms_DX12.DepthAlphaTestEnabled)
		std::sort(m_goListAT.begin(), m_goListAT.end(), sortNearToFar);
	else
		std::sort(m_goListAT.begin(), m_goListAT.end(), sortFarToNear);
}

vector<GameObject>& Batch::GetOpaques()
{
	return m_goList;
}

vector<GameObject>& Batch::GetATs()
{
	return m_goListAT;
}

vector<GameObject>& Batch::GetTrans()
{
	return m_goListTrans;
}

RootParamInfo& Batch::GetRoomParamInfo()
{
	return m_rootSig->GetRootParamInfo();
}
