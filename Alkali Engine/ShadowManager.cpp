#include "pch.h"
#include "ShadowManager.h"
#include "RootSig.h"
#include "AssetFactory.h"
#include "Scene.h"
#include "Batch.h"
#include "Frustum.h"
#include "ResourceTracker.h"

CascadeInfo ShadowManager::ms_cascadeInfos[MAX_SHADOW_MAP_CASCADES];
XMMATRIX ShadowManager::ms_viewMatrix;
XMMATRIX ShadowManager::ms_projMatrices[MAX_SHADOW_MAP_CASCADES];
XMMATRIX ShadowManager::ms_vpMatrices[MAX_SHADOW_MAP_CASCADES];
XMFLOAT3 ShadowManager::ms_maxBasis, ShadowManager::ms_forwardBasis;

ComPtr<ID3D12DescriptorHeap> ShadowManager::ms_dsvHeap;
UINT ShadowManager::ms_dsvDescriptorSize;
ComPtr<ID3D12Resource> ShadowManager::ms_shadowMapResource;

D3D12_RESOURCE_STATES ShadowManager::ms_currentDSVState;

std::unique_ptr<GameObject> ShadowManager::ms_viewGO;
shared_ptr<Shader> ShadowManager::ms_depthShader;
shared_ptr<RootSig> ShadowManager::ms_depthRootSig;
shared_ptr<Material> ShadowManager::ms_depthMat, ShadowManager::ms_viewDepthMat;
shared_ptr<RootSig> ShadowManager::ms_viewRootSig;

D3D12_VIEWPORT ShadowManager::ms_viewports[MAX_SHADOW_MAP_CASCADES];
vector<DebugLine*> ShadowManager::ms_debugLines;
bool ShadowManager::ms_initialised;

void ShadowManager::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, Frustum& frustum)
{
	if (ms_initialised)
		return;
	ms_initialised = true;

	assert(SettingsManager::ms_Dynamic.ShadowCascadeCount <= MAX_SHADOW_MAP_CASCADES);

	float shadowWidth = 1.0f / float(SettingsManager::ms_Dynamic.ShadowCascadeCount);

	for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
	{
		if (i >= SettingsManager::ms_Dynamic.ShadowCascadeCount)
		{
			SettingsManager::ms_Dynamic.ShadowNearPercents[i] = -1;
			SettingsManager::ms_Dynamic.ShadowFarPercents[i] = -1;
			continue;
		}

		SettingsManager::ms_Dynamic.ShadowNearPercents[i] = i * shadowWidth;
		SettingsManager::ms_Dynamic.ShadowFarPercents[i] = (i + 1) * shadowWidth;
	}

	{
		for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
		{
			float topLeftX = SettingsManager::ms_Misc.ShadowMapResoWidth * i;
			ms_viewports[i] = CD3DX12_VIEWPORT(topLeftX, 0.0f, static_cast<float>(SettingsManager::ms_Misc.ShadowMapResoWidth), static_cast<float>(SettingsManager::ms_Misc.ShadowMapResoHeight));
		}		

		ms_dsvHeap = ResourceManager::CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		ms_dsvDescriptorSize = d3d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_RESOURCE_DESC dsvDesc = {};
		dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsvDesc.Width = SettingsManager::ms_Misc.ShadowMapResoWidth * MAX_SHADOW_MAP_CASCADES;
		dsvDesc.Height = SettingsManager::ms_Misc.ShadowMapResoHeight;
		dsvDesc.DepthOrArraySize = 1;
		dsvDesc.MipLevels = 1;
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.SampleDesc.Count = 1;
		dsvDesc.SampleDesc.Quality = 0;
		dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0.0f;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = d3d->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &dsvDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&ms_shadowMapResource));
		ThrowIfFailed(hr);

		ms_currentDSVState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		//desc.Texture2DArray.ArraySize = MAX_SHADOW_MAP_CASCADES;

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(ms_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		d3d->GetDevice()->CreateDepthStencilView(ms_shadowMapResource.Get(), &desc, dsvHandle);
	}

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDepth =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	if (!ms_viewDepthMat)
	{
		RootParamInfo viewRPI;
		viewRPI.NumSRV_Dynamic = 1;
		viewRPI.NumCBV_PerFrame = 1;
		viewRPI.ParamIndexCBV_PerFrame = 0;
		viewRPI.ParamIndexSRV_Dynamic = 1;

		ms_viewRootSig = std::make_shared<RootSig>();
		ms_viewRootSig->InitDefaultSampler("View Shadow Map RS", viewRPI);

		ShaderArgs viewArgs = { L"DepthBuffer_VS.cso", L"DepthBuffer_PS.cso", inputLayoutDepth, ms_viewRootSig->GetRootSigResource() };
		viewArgs.cullNone = true;
		viewArgs.disableDSV = true;
		auto viewDepthShader = AssetFactory::CreateShader(viewArgs, true);

		ms_viewDepthMat = std::make_shared<Material>();
		vector<UINT> cbvFrameSizesView = { sizeof(DepthViewCB) };
		ms_viewDepthMat->AddCBVs(d3d, commandList, cbvFrameSizesView, true, "ShadowView");
		ms_viewDepthMat->AddDynamicSRVs("Shadow View", 1);

		auto modelPlane = AssetFactory::CreateModel("Plane.model", commandList);

		ms_viewGO = std::make_unique<GameObject>("View Shadow Map", modelPlane, viewDepthShader, ms_viewDepthMat, true);
		ms_viewGO->SetRotation(90, 0, 0);

		DepthViewCB dvCB;
		dvCB.Resolution = XMFLOAT2(SettingsManager::ms_Window.ScreenWidth, SettingsManager::ms_Window.ScreenHeight);
		dvCB.MaxValue = 1;
		dvCB.MinValue = 0.2f;

		ms_viewDepthMat->SetCBV_PerFrame(0, &dvCB, sizeof(DepthViewCB));
	}

	if (!ms_depthMat)
	{
		RootParamInfo depthRPI;
		depthRPI.NumCBV_PerDraw = 1;
		depthRPI.ParamIndexCBV_PerDraw = 0;

		ms_depthRootSig = std::make_shared<RootSig>();
		ms_depthRootSig->Init("Depth RS", depthRPI);

		ShaderArgs depthArgs = { L"Depth_VS.cso", L"", inputLayoutDepth, ms_depthRootSig->GetRootSigResource() };
		depthArgs.NoPS = true;
		depthArgs.cullFrontOnly = true;
		depthArgs.SlopeScaleDepthBias = 0.02f;
		ms_depthShader = AssetFactory::CreateShader(depthArgs, true);

		ms_depthMat = std::make_shared<Material>();
		vector<UINT> cbvDrawSizes = { sizeof(MatricesCB) };
		ms_depthMat->AddCBVs(d3d, commandList, cbvDrawSizes, false);
	}

	if (!SettingsManager::ms_Dynamic.ShadowMapUpdating)
	{
		CalculateBounds(XMFLOAT3(0, -1, 0), frustum);
		UpdateDebugLines(d3d);
	}		
}

void ShadowManager::Shutdown()
{
	ms_dsvHeap.Reset();
	ms_depthRootSig.reset();
	ms_depthShader.reset();
	ms_viewGO.reset();
	ms_viewDepthMat.reset();
	ms_depthMat.reset();
	ms_viewRootSig.reset();
	ms_shadowMapResource.Reset();

	ms_initialised = false;
}

void ShadowManager::Update(D3DClass* d3d, XMFLOAT3 lightDir, Frustum& frustum, const XMFLOAT3& eyePos)
{
	if (SettingsManager::ms_Dynamic.ShadowMapAutoNearFarPercents)
	{
		float shadowWidth = 1.0f / float(SettingsManager::ms_Dynamic.ShadowCascadeCount);

		for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
		{
			if (i >= SettingsManager::ms_Dynamic.ShadowCascadeCount)
			{
				SettingsManager::ms_Dynamic.ShadowNearPercents[i] = -1;
				SettingsManager::ms_Dynamic.ShadowFarPercents[i] = -1;
				continue;
			}

			SettingsManager::ms_Dynamic.ShadowNearPercents[i] = i * shadowWidth;
			SettingsManager::ms_Dynamic.ShadowFarPercents[i] = (i+1) * shadowWidth;
		}
	}

	XMFLOAT3 eye = XMFLOAT3(eyePos.x, 0, eyePos.z);
	XMVECTOR eyeV = XMLoadFloat3(&eye);

	XMFLOAT3 focus = Add(eye, lightDir);
	XMVECTOR focusV = XMLoadFloat3(&focus);

	XMFLOAT3 up = XMFLOAT3(0, 1, 0);
	XMVECTOR upV = XMLoadFloat3(&up);

	ms_viewMatrix = XMMatrixLookAtLH(eyeV, focusV, upV);

	if (!SettingsManager::ms_Dynamic.DynamicShadowMapBounds)
	{
		for (int i = 0; i < SettingsManager::ms_Dynamic.ShadowCascadeCount; i++)
		{
			ms_projMatrices[i] = XMMatrixOrthographicLH(ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);
			ms_vpMatrices[i] = ms_viewMatrix * ms_projMatrices[i];
		}
		return;
	}	

	if (SettingsManager::ms_Dynamic.ShadowMapUpdating)
	{
		CalculateBounds(lightDir, frustum);
		UpdateDebugLines(d3d);
	}		

	for (int i = 0; i < SettingsManager::ms_Dynamic.ShadowCascadeCount; i++)
	{
		ms_vpMatrices[i] = ms_viewMatrix * ms_projMatrices[i];
	}
}

void ShadowManager::CalculateBounds(XMFLOAT3 lightDir, Frustum& frustum) 
{	
	ms_forwardBasis = Normalize(lightDir);

	if (Equals(ms_forwardBasis, XMFLOAT3(0, -1, 0)))
		ms_forwardBasis = Normalize(XMFLOAT3(0.01f, -1, 0.01f));

	XMFLOAT3 rightBasis = Normalize(Cross(ms_forwardBasis, XMFLOAT3(0, 1, 0)));
	XMFLOAT3 upBasis = Normalize(Cross(ms_forwardBasis, rightBasis));
	ms_maxBasis = Mult(Normalize(Add(rightBasis, upBasis)), sqrt(2));

	BoundsArgs args = { ms_forwardBasis, ms_maxBasis, ResourceTracker::GetBatches(), frustum };

	for (int i = 0; i < SettingsManager::ms_Dynamic.ShadowCascadeCount; i++)
	{
		args.cascadeNear = SettingsManager::ms_Dynamic.ShadowNearPercents[i];
		args.cascadeFar = SettingsManager::ms_Dynamic.ShadowFarPercents[i];

		//CalculateBounds(args, ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);

		frustum.GetBoundingBoxFromDir(lightDir, args.cascadeNear, args.cascadeFar, ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);

		ms_projMatrices[i] = XMMatrixOrthographicLH(ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);
		ms_vpMatrices[i] = ms_viewMatrix * ms_projMatrices[i];
	}
}

void ShadowManager::CalculateSceneBounds(BoundsArgs args, float& width, float& height, float& nearDist, float& farDist)
{
	// pMax = up * r + right * r = (up + right) * r * sqrt(2)
	// pMax = [(up + right) * sqrt(2)] * r

	width = 0;
	height = 0;
	nearDist = INFINITY;
	farDist = -INFINITY;

	for (auto& it : args.batchList)
	{
		auto& opaques = it.second->GetOpaques();
		auto& trans = it.second->GetTrans();

		for (size_t i = 0; i < opaques.size(); i++)
		{
			XMFLOAT3 pos;
			float radius;
			opaques[i].GetBoundingSphere(pos, radius);

			if (!args.frustum.CheckSphere(pos, radius, args.cascadeNear, args.cascadeFar))
				continue;

			XMFLOAT3 pMax = Add(Abs(pos), Abs(Mult(args.maxBasis, radius)));

			width = std::max(width, pMax.x);
			height = std::max(height, pMax.y);

			XMFLOAT3 pForward = Mult(args.forwardBasis, pos.z);

			nearDist = std::min(nearDist, pForward.z - radius);
			farDist = std::max(farDist, pForward.z + radius);
		}

		for (size_t i = 0; i < trans.size(); i++)
		{
			XMFLOAT3 pos;
			float radius;
			trans[i].GetBoundingSphere(pos, radius);			

			if (!args.frustum.CheckSphere(pos, radius, args.cascadeNear, args.cascadeFar))
				continue;

			XMFLOAT3 pMax = Add(Abs(pos), Abs(Mult(args.maxBasis, radius)));

			width = std::max(width, pMax.x);
			height = std::max(height, pMax.y);

			XMFLOAT3 pForward = Mult(args.forwardBasis, pos.z);

			nearDist = std::min(nearDist, pForward.z - radius);
			farDist = std::max(farDist, pForward.z + radius);
		}
	}

	width *= 2;
	height *= 2;
}

void ShadowManager::Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, unordered_map<string, shared_ptr<Batch>>& batchList, Frustum& frustum)
{
	if (ms_currentDSVState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		ResourceManager::TransitionResource(commandList, ms_shadowMapResource.Get(), ms_currentDSVState, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	auto dsvHandle = ms_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	RenderOverride ro = { ms_depthShader.get(), ms_depthRootSig.get() };
	ro.UseShadowMapMat = true;
	ro.CullAgainstBounds = true;
	ro.MaxBasis = Abs(ms_maxBasis);
	ro.ForwardBasis = ms_forwardBasis;

	XMMATRIX v = ms_viewMatrix;

	for (int i = 0; i < SettingsManager::ms_Dynamic.ShadowCascadeCount; i++)
	{
		commandList->RSSetViewports(1, &ms_viewports[i]);				

		XMMATRIX p = ms_projMatrices[i];

		ro.CascadeIndex = i;
		ro.BoundsWidth = ms_cascadeInfos[i].Width;
		ro.BoundsHeight = ms_cascadeInfos[i].Height;
		ro.BoundsNear = ms_cascadeInfos[i].Near;
		ro.BoundsFar = ms_cascadeInfos[i].Far;

		for (auto& it : batchList)
		{	
			it.second->Render(d3d, commandList, v, p, nullptr, &ro);
			it.second->RenderTrans(d3d, commandList, v, p, nullptr, &ro);
		}
	}

	ResourceManager::TransitionResource(commandList, ms_shadowMapResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, SettingsManager::ms_DX12.SRVFormat);
	ms_currentDSVState = SettingsManager::ms_DX12.SRVFormat;
}

ID3D12Resource* ShadowManager::GetShadowMap()
{
	return ms_shadowMapResource.Get();
}

XMMATRIX* ShadowManager::GetVPMatrices()
{
	return ms_vpMatrices;
}

XMFLOAT4 ShadowManager::GetCascadeDistances()
{
	float nearFarDist = SettingsManager::ms_Dynamic.FarPlane - SettingsManager::ms_Dynamic.NearPlane;

	XMFLOAT4 dist = XMFLOAT4(0, 0, 0, 0);
	if (SettingsManager::ms_Dynamic.ShadowCascadeCount > 0)
		dist.x = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.ShadowNearPercents[0];
	if (SettingsManager::ms_Dynamic.ShadowCascadeCount > 1)
		dist.y = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.ShadowNearPercents[1];
	if (SettingsManager::ms_Dynamic.ShadowCascadeCount > 2)
		dist.z = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.ShadowNearPercents[2];
	if (SettingsManager::ms_Dynamic.ShadowCascadeCount > 3)
		dist.w = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.ShadowNearPercents[3];

	return dist;
}

float ShadowManager::GetPCFSampleRange(int sampleCount)
{
	switch (sampleCount)
	{
	case 1:
		return 0;
	case 4:
		return 0.5f;
	case 16:
		return 1.5f;
	default:
		throw std::exception("Invalid sample count");
	}
}

void ShadowManager::RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle)
{
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr); // Disable DSV

	commandList->SetGraphicsRootSignature(ms_viewRootSig->GetRootSigResource());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ms_viewDepthMat->SetDynamicSRV(d3d, 0, DXGI_FORMAT_R32_FLOAT, ms_shadowMapResource.Get());

	ms_viewGO->Render(d3d, commandList, ms_viewRootSig->GetRootParamInfo());

	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}

void ShadowManager::SetDebugLines(vector<DebugLine*>& debugLines)
{
	assert(debugLines.size() == MAX_SHADOW_MAP_CASCADES * 12);

	ms_debugLines = debugLines;
}

void ShadowManager::UpdateDebugLines(D3DClass* d3d)
{
	if (!SettingsManager::ms_Dynamic.ShadowBoundsDebugLinesEnabled || ms_debugLines.size() == 0)
		return;

	XMFLOAT3 rightBasis = Normalize(Cross(ms_forwardBasis, XMFLOAT3(0, 1, 0)));
	XMFLOAT3 upBasis = Normalize(Cross(ms_forwardBasis, rightBasis));

	for (int i = 0; i < SettingsManager::ms_Dynamic.ShadowCascadeCount; i++)
	{
		XMFLOAT3 x = Mult(rightBasis, ms_cascadeInfos[i].Width * 0.5f);
		XMFLOAT3 y = Mult(upBasis, ms_cascadeInfos[i].Height * 0.5f);
		XMFLOAT3 zF = Mult(ms_forwardBasis, ms_cascadeInfos[i].Far);
		XMFLOAT3 zN = Mult(ms_forwardBasis, ms_cascadeInfos[i].Near);

		int lineI = i * 12;

		ms_debugLines[lineI + 0]->SetPositions(d3d, Add(Add(x, y), zN), Add(Add(Negate(x), y), zN));
		ms_debugLines[lineI + 1]->SetPositions(d3d, Add(Add(x, y), zN), Add(Add(x, Negate(y)), zN));
		ms_debugLines[lineI + 2]->SetPositions(d3d, Add(Add(Negate(x), y), zN), Add(Add(Negate(x), Negate(y)), zN));
		ms_debugLines[lineI + 3]->SetPositions(d3d, Add(Add(x, Negate(y)), zN), Add(Add(Negate(x), Negate(y)), zN));

		ms_debugLines[lineI + 4]->SetPositions(d3d, Add(Add(x, y), zF), Add(Add(Negate(x), y), zF));
		ms_debugLines[lineI + 5]->SetPositions(d3d, Add(Add(x, y), zF), Add(Add(x, Negate(y)), zF));
		ms_debugLines[lineI + 6]->SetPositions(d3d, Add(Add(Negate(x), y), zF), Add(Add(Negate(x), Negate(y)), zF));
		ms_debugLines[lineI + 7]->SetPositions(d3d, Add(Add(x, Negate(y)), zF), Add(Add(Negate(x), Negate(y)), zF));

		ms_debugLines[lineI + 8]->SetPositions(d3d, Add(Add(x, y), zN), Add(Add(x, y), zF));
		ms_debugLines[lineI + 9]->SetPositions(d3d, Add(Add(Negate(x), Negate(y)), zN), Add(Add(Negate(x), Negate(y)), zF));
		ms_debugLines[lineI + 10]->SetPositions(d3d, Add(Add(Negate(x), y), zN), Add(Add(Negate(x), y), zF));
		ms_debugLines[lineI + 11]->SetPositions(d3d, Add(Add(x, Negate(y)), zN), Add(Add(x, Negate(y)), zF));
	}
}