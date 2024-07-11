#include "pch.h"
#include "ShadowManager.h"
#include "RootSig.h"
#include "AssetFactory.h"
#include "Scene.h"
#include "Batch.h"
#include "Frustum.h"
#include "ResourceTracker.h"

CascadeInfo ShadowManager::ms_cascadeInfos[SHADOW_MAP_CASCADES];
XMMATRIX ShadowManager::ms_viewMatrix;
XMMATRIX ShadowManager::ms_vpMatrices[SHADOW_MAP_CASCADES];

ComPtr<ID3D12DescriptorHeap> ShadowManager::ms_dsvHeap;
UINT ShadowManager::ms_dsvDescriptorSize;
ComPtr<ID3D12Resource> ShadowManager::ms_shadowMapResource;

D3D12_RESOURCE_STATES ShadowManager::ms_currentDSVState;

std::unique_ptr<GameObject> ShadowManager::ms_viewGO;
shared_ptr<Shader> ShadowManager::ms_depthShader;
shared_ptr<RootSig> ShadowManager::ms_depthRootSig;
shared_ptr<Material> ShadowManager::ms_depthMat, ShadowManager::ms_viewDepthMat;
shared_ptr<RootSig> ShadowManager::ms_viewRootSig;

D3D12_VIEWPORT ShadowManager::ms_viewports[SHADOW_MAP_CASCADES];

void ShadowManager::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList)
{
	ms_cascadeInfos[0] = { 0.0f, 0.5f };
	ms_cascadeInfos[1] = { 0.5f, 1.0f };

	{
		for (int i = 0; i < SHADOW_MAP_CASCADES; i++)
		{
			float topLeftX = SettingsManager::ms_Dynamic.ShadowMapResoWidth * i;
			ms_viewports[i] = CD3DX12_VIEWPORT(topLeftX, 0.0f, static_cast<float>(SettingsManager::ms_Dynamic.ShadowMapResoWidth), static_cast<float>(SettingsManager::ms_Dynamic.ShadowMapResoHeight));
		}		

		ms_dsvHeap = ResourceManager::CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		ms_dsvDescriptorSize = d3d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_RESOURCE_DESC dsvDesc = {};
		dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsvDesc.Width = SettingsManager::ms_Dynamic.ShadowMapResoWidth * SHADOW_MAP_CASCADES;
		dsvDesc.Height = SettingsManager::ms_Dynamic.ShadowMapResoHeight;		
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
		desc.Texture2DArray.ArraySize = SHADOW_MAP_CASCADES;

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(ms_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		d3d->GetDevice()->CreateDepthStencilView(ms_shadowMapResource.Get(), &desc, dsvHandle);
	}

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDepth =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

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

	{
		RootParamInfo depthRPI;
		depthRPI.NumCBV_PerDraw = 1;
		depthRPI.ParamIndexCBV_PerDraw = 0;

		ms_depthRootSig = std::make_shared<RootSig>();
		ms_depthRootSig->Init("Depth RS", depthRPI);

		ShaderArgs depthArgs = { L"Depth_VS.cso", L"", inputLayoutDepth, ms_depthRootSig->GetRootSigResource() };
		depthArgs.NoPS = true;
		depthArgs.cullFrontOnly = true;
		ms_depthShader = AssetFactory::CreateShader(depthArgs, true);

		ms_depthMat = std::make_shared<Material>();
		vector<UINT> cbvDrawSizes = { sizeof(MatricesCB) };
		ms_depthMat->AddCBVs(d3d, commandList, cbvDrawSizes, false);
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
}

void ShadowManager::Update(XMFLOAT3 lightDir, Frustum& frustum)
{
	XMVECTOR eyeV = XMLoadFloat3(&XMFLOAT3_ZERO);

	XMVECTOR focusV = XMLoadFloat3(&lightDir);

	XMFLOAT3 up = XMFLOAT3(0, 1, 0);
	XMVECTOR upV = XMLoadFloat3(&up);

	ms_viewMatrix = XMMatrixLookAtLH(eyeV, focusV, upV);

	if (!SettingsManager::ms_Dynamic.DynamicShadowMapBounds)
	{
		for (int i = 0; i < SHADOW_MAP_CASCADES; i++)
		{
			XMMATRIX proj = XMMatrixOrthographicLH(ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);
			ms_vpMatrices[i] = ms_viewMatrix * proj;
		}
		return;
	}

	XMFLOAT3 forwardBasis = Normalize(lightDir);
	XMFLOAT3 rightBasis = Normalize(Cross(forwardBasis, XMFLOAT3(0, 1, 0)));
	XMFLOAT3 upBasis = Normalize(Cross(forwardBasis, rightBasis));
	XMFLOAT3 maxBasis = Mult(Normalize(Add(rightBasis, upBasis)), sqrt(2));	

	BoundsArgs args = { forwardBasis, maxBasis, ResourceTracker::GetBatches(), frustum };

	for (int i = 0; i < SHADOW_MAP_CASCADES; i++)
	{
		args.cascadeNear = ms_cascadeInfos[i].NearPercent;
		args.cascadeFar = ms_cascadeInfos[i].FarPercent;

		//CalculateBounds(args, ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);

		frustum.GetBoundingBoxFromDir(lightDir, args.cascadeNear, args.cascadeFar, ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);

		XMMATRIX proj = XMMatrixOrthographicLH(ms_cascadeInfos[i].Width, ms_cascadeInfos[i].Height, ms_cascadeInfos[i].Near, ms_cascadeInfos[i].Far);
		ms_vpMatrices[i] = ms_viewMatrix * proj;
	}
}

void ShadowManager::CalculateBounds(BoundsArgs args, float& width, float& height, float& nearDist, float& farDist)
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

			nearDist = std::min(nearDist, pos.z - radius);
			farDist = std::max(farDist, pos.z + radius);
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

			XMFLOAT3 pN = Add(pos, Mult(args.forwardBasis, -radius));
			XMFLOAT3 pF = Add(pos, Mult(args.forwardBasis, radius));

			nearDist = std::min(nearDist, pN.z);
			farDist = std::max(farDist, pF.z);
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

	for (int i = 0; i < SHADOW_MAP_CASCADES; i++)
	{
		commandList->RSSetViewports(1, &ms_viewports[i]);			

		for (auto& it : batchList)
		{
			ro.FrustumNearPercent = ms_cascadeInfos[i].NearPercent;
			ro.FrustumFarPercent = ms_cascadeInfos[i].FarPercent;

			it.second->Render(d3d, commandList, ms_vpMatrices[i], &frustum, &ro);
			it.second->RenderTrans(d3d, commandList, ms_vpMatrices[i], &frustum, &ro);
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

void ShadowManager::RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle)
{
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr); // Disable DSV

	commandList->SetGraphicsRootSignature(ms_viewRootSig->GetRootSigResource());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ms_viewDepthMat->SetDynamicSRV(d3d, 0, DXGI_FORMAT_R32_FLOAT, ms_shadowMapResource.Get());

	ms_viewGO->Render(d3d, commandList, ms_viewRootSig->GetRootParamInfo());

	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}
