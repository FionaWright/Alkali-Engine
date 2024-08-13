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
XMFLOAT3 ShadowManager::ms_maxBasis, ShadowManager::ms_forwardBasis, ShadowManager::ms_eyePos;

ComPtr<ID3D12DescriptorHeap> ShadowManager::ms_dsvHeap;
UINT ShadowManager::ms_dsvDescriptorSize;
ComPtr<ID3D12Resource> ShadowManager::ms_shadowMapResource;

D3D12_RESOURCE_STATES ShadowManager::ms_currentDSVState;

std::unique_ptr<GameObject> ShadowManager::ms_viewGO;
shared_ptr<Shader> ShadowManager::ms_depthShader;
shared_ptr<RootSig> ShadowManager::ms_depthRootSig;
shared_ptr<Material> ShadowManager::ms_viewDepthMat;
shared_ptr<RootSig> ShadowManager::ms_viewRootSig;

D3D12_VIEWPORT ShadowManager::ms_viewports[MAX_SHADOW_MAP_CASCADES];
vector<DebugLine*> ShadowManager::ms_debugLines;
bool ShadowManager::ms_initialised;

void ShadowManager::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, Frustum& frustum)
{
	if (ms_initialised)
		return;
	ms_initialised = true;

	assert(SettingsManager::ms_Dynamic.Shadow.CascadeCount <= MAX_SHADOW_MAP_CASCADES);

	CalculateNearFarPercents();

	{
		for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
		{
			float topLeftX = SettingsManager::ms_Misc.ShadowMapResoWidth * i;
			ms_viewports[i] = CD3DX12_VIEWPORT(topLeftX, 0.0f, static_cast<float>(SettingsManager::ms_Misc.ShadowMapResoWidth), static_cast<float>(SettingsManager::ms_Misc.ShadowMapResoHeight));
		}		

		ms_dsvHeap = ResourceManager::CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		ms_dsvHeap->SetName(L"Shadow DSV Heap");

		ms_dsvDescriptorSize = d3d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_RESOURCE_DESC dsvDesc = {};
		dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsvDesc.Width = SettingsManager::ms_Misc.ShadowMapResoWidth * MAX_SHADOW_MAP_CASCADES;
		dsvDesc.Height = SettingsManager::ms_Misc.ShadowMapResoHeight;
		dsvDesc.DepthOrArraySize = 1;
		dsvDesc.MipLevels = 1;
		dsvDesc.Format = SettingsManager::ms_Misc.ShadowHDFormat ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D16_UNORM;
		dsvDesc.SampleDesc.Count = 1;
		dsvDesc.SampleDesc.Quality = 0;
		dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = dsvDesc.Format;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0.0f;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = d3d->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &dsvDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&ms_shadowMapResource));
		ThrowIfFailed(hr);

		ms_currentDSVState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = dsvDesc.Format;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(ms_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		d3d->GetDevice()->CreateDepthStencilView(ms_shadowMapResource.Get(), &desc, dsvHandle);

		ms_shadowMapResource->SetName(L"Shadow Map Resource");
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
		ms_viewRootSig->Init("View Shadow Map RS", viewRPI, &SettingsManager::ms_DX12.DefaultSamplerDesc, 1);

		ShaderArgs viewArgs = { L"DepthBuffer_VS.cso", L"DepthBuffer_PS.cso", inputLayoutDepth, ms_viewRootSig->GetRootSigResource() };
		viewArgs.CullNone = true;
		viewArgs.disableDSV = true;
		auto viewDepthShader = AssetFactory::CreateShader(viewArgs, true);

		ms_viewDepthMat = AssetFactory::CreateMaterial();
		vector<UINT> cbvFrameSizesView = { sizeof(DepthViewCB) };
		ms_viewDepthMat->AddCBVs(d3d, cmdList, cbvFrameSizesView, true, "ShadowView");
		ms_viewDepthMat->AddDynamicSRVs("Shadow View", 1);

		auto modelPlane = AssetFactory::CreateModel("Plane.model", cmdList);

		ms_viewGO = std::make_unique<GameObject>("View Shadow Map", modelPlane, viewDepthShader, ms_viewDepthMat, true);
		ms_viewGO->SetRotation(90, 0, 0);

		DepthViewCB dvCB;
		dvCB.Resolution = XMFLOAT2(SettingsManager::ms_Window.ScreenWidth, SettingsManager::ms_Window.ScreenHeight);
		dvCB.MaxValue = 1;
		dvCB.MinValue = 0.2f;

		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
			ms_viewDepthMat->SetCBV_PerFrame(0, &dvCB, sizeof(DepthViewCB), i);

		DXGI_FORMAT format = SettingsManager::ms_Misc.ShadowHDFormat ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM;
		ms_viewDepthMat->SetDynamicSRV(d3d, 0, format, ms_shadowMapResource.Get());
	}

	if (!ms_depthShader)
	{
		RootParamInfo depthRPI;
		depthRPI.NumCBV_PerDraw = 1;
		depthRPI.ParamIndexCBV_PerDraw = 0;

		ms_depthRootSig = std::make_shared<RootSig>();
		ms_depthRootSig->Init("Depth RS", depthRPI);

		ShaderArgs depthArgs = { L"Depth_VS.cso", L"", inputLayoutDepth, ms_depthRootSig->GetRootSigResource() };
		depthArgs.NoPS = true;
		depthArgs.CullFront = SettingsManager::ms_Misc.ShadowCullFront;
		//depthArgs.SlopeScaleDepthBias = -0.02f;
		depthArgs.DSVFormat = SettingsManager::ms_Misc.ShadowHDFormat ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D16_UNORM;
		ms_depthShader = AssetFactory::CreateShader(depthArgs, true);
	}

	if (!SettingsManager::ms_Dynamic.Shadow.UpdatingBounds)
	{
		CalculateBoundsAndMatrices(XMFLOAT3_ZERO, XMFLOAT3(0, -1, 0), frustum);
		UpdateDebugLines(d3d, XMFLOAT3_ZERO);
	}		
}

void ShadowManager::Shutdown()
{
	ms_dsvHeap.Reset();
	ms_depthRootSig.reset();
	ms_depthShader.reset();
	ms_viewGO.reset();
	ms_viewDepthMat.reset();
	ms_viewRootSig.reset();
	ms_shadowMapResource.Reset();

	ms_initialised = false;
}

void ShadowManager::Update(D3DClass* d3d, XMFLOAT3 lightDir, Frustum& frustum, const XMFLOAT3& eyePos)
{
	if (SettingsManager::ms_Dynamic.Shadow.AutoNearFarPercent)
		CalculateNearFarPercents();

	ms_eyePos = eyePos;
	XMVECTOR eyeV = XMLoadFloat3(&ms_eyePos);

	XMFLOAT3 focus = Add(ms_eyePos, lightDir);
	XMVECTOR focusV = XMLoadFloat3(&focus);

	XMFLOAT3 up = XMFLOAT3(0, 1, 0);
	XMVECTOR upV = XMLoadFloat3(&up);

	ms_viewMatrix = XMMatrixLookAtLH(eyeV, focusV, upV);

	if (SettingsManager::ms_Dynamic.Shadow.UpdatingBounds)
	{
		CalculateBoundsAndMatrices(ms_eyePos, lightDir, frustum);
		UpdateDebugLines(d3d, ms_eyePos);
	}		

	for (int i = 0; i < SettingsManager::ms_Dynamic.Shadow.CascadeCount; i++)
	{
		ms_vpMatrices[i] = ms_viewMatrix * ms_projMatrices[i];
	}
}

void ShadowManager::CalculateNearFarPercents() 
{
	float base = 2;
	float sum = 0;

	for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
	{
		if (i >= SettingsManager::ms_Dynamic.Shadow.CascadeCount)
		{
			SettingsManager::ms_Dynamic.Shadow.NearPercents[i] = NAN;
			SettingsManager::ms_Dynamic.Shadow.FarPercents[i] = NAN;
			continue;
		}
		
		if (i == 0)
			SettingsManager::ms_Dynamic.Shadow.NearPercents[i] = 0;
		else
			SettingsManager::ms_Dynamic.Shadow.NearPercents[i] = pow(base, i);

		SettingsManager::ms_Dynamic.Shadow.FarPercents[i] = pow(base, i+1);
		sum += SettingsManager::ms_Dynamic.Shadow.FarPercents[i] - SettingsManager::ms_Dynamic.Shadow.NearPercents[i];
	}

	for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
	{
		if (i >= SettingsManager::ms_Dynamic.Shadow.CascadeCount)
			continue;

		SettingsManager::ms_Dynamic.Shadow.NearPercents[i] /= sum;
		SettingsManager::ms_Dynamic.Shadow.FarPercents[i] /= sum;
	}
}

void ShadowManager::CalculateBoundsAndMatrices(const XMFLOAT3& eyePos, XMFLOAT3 lightDir, Frustum& frustum) 
{	
	ms_forwardBasis = Normalize(lightDir);

	if (Equals(ms_forwardBasis, XMFLOAT3(0, -1, 0)))
		ms_forwardBasis = Normalize(XMFLOAT3(0.01f, -1, 0.01f));

	XMFLOAT3 rightBasis = Normalize(Cross(ms_forwardBasis, XMFLOAT3(0, 1, 0)));
	XMFLOAT3 upBasis = Normalize(Cross(ms_forwardBasis, rightBasis));
	ms_maxBasis = Mult(Normalize(Add(rightBasis, upBasis)), sqrt(2));

	BoundsArgs args = { ms_forwardBasis, ms_maxBasis, ResourceTracker::GetBatches(), frustum };

	for (int i = 0; i < SettingsManager::ms_Dynamic.Shadow.CascadeCount; i++)
	{
		args.cascadeNear = SettingsManager::ms_Dynamic.Shadow.NearPercents[i];
		args.cascadeFar = SettingsManager::ms_Dynamic.Shadow.FarPercents[i];

		float sceneWidth, sceneHeight, sceneNear, sceneFar;
		CalculateSceneBounds(args, eyePos, sceneWidth, sceneHeight, sceneNear, sceneFar);

		if (SettingsManager::ms_Dynamic.Shadow.UseBoundingSpheres)
			frustum.GetBoundingSphereFromDir(args.cascadeNear, args.cascadeFar, ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);
		else
			frustum.GetBoundingBoxFromDir(eyePos, lightDir, args.cascadeNear, args.cascadeFar, ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);		

		ms_cascadeInfos[i].Width += SettingsManager::ms_Dynamic.Shadow.BoundsBias;
		ms_cascadeInfos[i].Height += SettingsManager::ms_Dynamic.Shadow.BoundsBias;

		if (SettingsManager::ms_Dynamic.Shadow.BoundToScene)
		{
			ms_cascadeInfos[i].Width = std::max(ms_cascadeInfos[i].Width, sceneWidth);
			ms_cascadeInfos[i].Height = std::max(ms_cascadeInfos[i].Height, sceneHeight);
		}

		ms_cascadeInfos[i].Near = std::min(ms_cascadeInfos[i].Near, sceneNear);
		ms_cascadeInfos[i].Far = std::max(ms_cascadeInfos[i].Far, sceneFar);

		double worldUnitsPerTexelX = ms_cascadeInfos[i].Width / static_cast<double>(SettingsManager::ms_Misc.ShadowMapResoWidth);
		double worldUnitsPerTexelY = ms_cascadeInfos[i].Height / static_cast<double>(SettingsManager::ms_Misc.ShadowMapResoHeight);
		ms_cascadeInfos[i].Width = std::floor(ms_cascadeInfos[i].Width / worldUnitsPerTexelX) * worldUnitsPerTexelX;
		ms_cascadeInfos[i].Height = std::floor(ms_cascadeInfos[i].Height / worldUnitsPerTexelY) * worldUnitsPerTexelY;

		ms_projMatrices[i] = XMMatrixOrthographicLH(ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);
		ms_vpMatrices[i] = ms_viewMatrix * ms_projMatrices[i];
	}
}

void ShadowManager::CalculateSceneBounds(BoundsArgs args, const XMFLOAT3& eyePos, float& width, float& height, float& nearDist, float& farDist)
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

			pos = Subtract(pos, eyePos);

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

			pos = Subtract(pos, eyePos);

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

void ShadowManager::Render(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, unordered_map<string, shared_ptr<Batch>>& batchList, Frustum& frustum, const int& backBufferIndex)
{
	if (!SettingsManager::ms_Dynamic.Shadow.Rendering)
		return;

	if (ms_currentDSVState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		ResourceManager::TransitionResource(cmdList, ms_shadowMapResource.Get(), ms_currentDSVState, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	auto dsvHandle = ms_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	RenderOverride ro = { ms_depthShader.get(), ms_depthRootSig.get() };
	ro.UseShadowMapMat = true;
	ro.CullAgainstBounds = SettingsManager::ms_Dynamic.Shadow.CullAgainstBounds;
	ro.MaxBasis = Abs(ms_maxBasis);
	ro.ForwardBasis = ms_forwardBasis;

	XMMATRIX v = ms_viewMatrix;

	for (int i = 0; i < SettingsManager::ms_Dynamic.Shadow.CascadeCount; i++)
	{
		cmdList->RSSetViewports(1, &ms_viewports[i]);				

		XMMATRIX p = ms_projMatrices[i];

		ro.CascadeIndex = i;
		ro.BoundsWidth = ms_cascadeInfos[i].Width;
		ro.BoundsHeight = ms_cascadeInfos[i].Height;
		ro.BoundsNear = ms_cascadeInfos[i].Near;
		ro.BoundsFar = ms_cascadeInfos[i].Far;
		ro.Origin = ms_eyePos;

		for (auto& it : batchList)
		{	
			it.second->Render(d3d, cmdList, backBufferIndex, v, p, nullptr, &ro);
			it.second->RenderTrans(d3d, cmdList, backBufferIndex, v, p, nullptr, &ro);
		}
	}

	ResourceManager::TransitionResource(cmdList, ms_shadowMapResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, SettingsManager::ms_DX12.SRVFormat);
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
	if (SettingsManager::ms_Dynamic.Shadow.CascadeCount > 0)
		dist.x = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.Shadow.NearPercents[0];
	if (SettingsManager::ms_Dynamic.Shadow.CascadeCount > 1)
		dist.y = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.Shadow.NearPercents[1];
	if (SettingsManager::ms_Dynamic.Shadow.CascadeCount > 2)
		dist.z = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.Shadow.NearPercents[2];
	if (SettingsManager::ms_Dynamic.Shadow.CascadeCount > 3)
		dist.w = SettingsManager::ms_Dynamic.NearPlane + nearFarDist * SettingsManager::ms_Dynamic.Shadow.NearPercents[3];

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

void ShadowManager::RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle, const int& backBufferIndex)
{
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr); // Disable DSV

	cmdList->SetGraphicsRootSignature(ms_viewRootSig->GetRootSigResource());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ms_viewGO->Render(d3d, cmdList, ms_viewRootSig->GetRootParamInfo(), backBufferIndex);

	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}

void ShadowManager::SetDebugLines(vector<DebugLine*>& debugLines)
{
	assert(debugLines.size() == MAX_SHADOW_MAP_CASCADES * 12);

	ms_debugLines = debugLines;
}

void ShadowManager::UpdateDebugLines(D3DClass* d3d, const XMFLOAT3& eyePos)
{
	if (!SettingsManager::ms_Dynamic.Shadow.ShowDebugBounds || ms_debugLines.size() == 0)
		return;

	XMFLOAT3 rightBasis = Normalize(Cross(ms_forwardBasis, XMFLOAT3(0, 1, 0)));
	XMFLOAT3 upBasis = Normalize(Cross(ms_forwardBasis, rightBasis));

	for (int i = 0; i < SettingsManager::ms_Dynamic.Shadow.CascadeCount; i++)
	{
		XMFLOAT3 xe = Mult(rightBasis, ms_cascadeInfos[i].Width * 0.5f);
		XMFLOAT3 nxe = Subtract(eyePos, xe);
		xe = Add(eyePos, xe);

		XMFLOAT3 y = Mult(upBasis, ms_cascadeInfos[i].Height * 0.5f);
		XMFLOAT3 ny = Negate(y);

		XMFLOAT3 zF = Mult(ms_forwardBasis, ms_cascadeInfos[i].Far);
		XMFLOAT3 zN = Mult(ms_forwardBasis, ms_cascadeInfos[i].Near);

		int lineI = i * 12;

		ms_debugLines[lineI + 0]->SetPositions(d3d, Add(Add(xe, y), zN), Add(Add(nxe, y), zN));
		ms_debugLines[lineI + 1]->SetPositions(d3d, Add(Add(xe, y), zN), Add(Add(xe, ny), zN));
		ms_debugLines[lineI + 2]->SetPositions(d3d, Add(Add(nxe, y), zN), Add(Add(nxe, ny), zN));
		ms_debugLines[lineI + 3]->SetPositions(d3d, Add(Add(xe, ny), zN), Add(Add(nxe, ny), zN));

		ms_debugLines[lineI + 4]->SetPositions(d3d, Add(Add(xe, y), zF), Add(Add(nxe, y), zF));
		ms_debugLines[lineI + 5]->SetPositions(d3d, Add(Add(xe, y), zF), Add(Add(xe, ny), zF));
		ms_debugLines[lineI + 6]->SetPositions(d3d, Add(Add(nxe, y), zF), Add(Add(nxe, ny), zF));
		ms_debugLines[lineI + 7]->SetPositions(d3d, Add(Add(xe, ny), zF), Add(Add(nxe, ny), zF));

		ms_debugLines[lineI + 8]->SetPositions(d3d, Add(Add(xe, y), zN), Add(Add(xe, y), zF));
		ms_debugLines[lineI + 9]->SetPositions(d3d, Add(Add(nxe, ny), zN), Add(Add(nxe, ny), zF));
		ms_debugLines[lineI + 10]->SetPositions(d3d, Add(Add(nxe, y), zN), Add(Add(nxe, y), zF));
		ms_debugLines[lineI + 11]->SetPositions(d3d, Add(Add(xe, ny), zN), Add(Add(xe, ny), zF));
	}
}