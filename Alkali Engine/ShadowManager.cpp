#include "pch.h"
#include "ShadowManager.h"
#include "RootSig.h"
#include "AssetFactory.h"
#include "Scene.h"
#include "Batch.h"
#include "Frustum.h"

XMMATRIX ShadowManager::ms_viewMatrix, ShadowManager::ms_projMatrix, ShadowManager::ms_vpMatrix;

ComPtr<ID3D12DescriptorHeap> ShadowManager::ms_dsvHeap;
UINT ShadowManager::ms_dsvDescriptorSize;
ComPtr<ID3D12Resource> ShadowManager::ms_shadowMapResource;

D3D12_RESOURCE_STATES ShadowManager::ms_currentDSVState;

std::unique_ptr<GameObject> ShadowManager::ms_viewGO;
shared_ptr<Shader> ShadowManager::ms_depthShader;
shared_ptr<RootSig> ShadowManager::ms_depthRootSig;
shared_ptr<Material> ShadowManager::ms_depthMat;

void ShadowManager::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList)
{
	{
		ms_dsvHeap = ResourceManager::CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		ms_dsvDescriptorSize = d3d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_RESOURCE_DESC dsvDesc = {};
		dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsvDesc.Width = SettingsManager::ms_DX12.ShadowMapWidth;
		dsvDesc.Height = SettingsManager::ms_DX12.ShadowMapHeight;
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

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(ms_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		d3d->GetDevice()->CreateDepthStencilView(ms_shadowMapResource.Get(), &desc, dsvHandle);
	}

	RootParamInfo viewRPI;
	viewRPI.NumSRV_Dynamic = 1;
	viewRPI.ParamIndexSRV_Dynamic = 0;

	auto viewRootSig = std::make_shared<RootSig>();
	viewRootSig->InitDefaultSampler("View Shadow Map RS", viewRPI);

	RootParamInfo depthRPI;
	depthRPI.NumCBV_PerDraw = 1;
	depthRPI.ParamIndexCBV_PerDraw = 0;

	ms_depthRootSig = std::make_shared<RootSig>();
	ms_depthRootSig->Init("Depth RS", depthRPI);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDepth =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	ShaderArgs viewArgs = { L"DepthBuffer_VS.cso", L"DepthBuffer_PS.cso", inputLayoutDepth, viewRootSig->GetRootSigResource() };
	viewArgs.cullNone = true;
	viewArgs.disableDSV = true;
	auto viewDepthShader = AssetFactory::CreateShader(viewArgs, true);

	ShaderArgs depthArgs = { L"Depth_VS.cso", L"", inputLayoutDepth, ms_depthRootSig->GetRootSigResource() };
	depthArgs.NoPS = true;
	ms_depthShader = AssetFactory::CreateShader(depthArgs, true);

	ms_depthMat = std::make_shared<Material>();
	vector<UINT> cbvDrawSizes = { sizeof(MatricesCB) };
	ms_depthMat->AddCBVs(d3d, commandList, cbvDrawSizes, false);

	auto viewDepthmat = std::make_shared<Material>();
	viewDepthmat->AddDynamicSRVs("Shadow View", 1);

	auto modelPlane = AssetFactory::CreateModel("Plane.model", commandList);

	ms_viewGO = std::make_unique<GameObject>("View Shadow Map", modelPlane, viewDepthShader, viewDepthmat, true);
	ms_viewGO->SetRotation(90, 0, 0);
}

void ShadowManager::Shutdown()
{
	ms_dsvHeap.Reset();
	ms_depthRootSig.reset();
	ms_depthShader.reset();
	ms_viewGO.reset();
	ms_shadowMapResource.Reset();
}

void ShadowManager::Update(XMFLOAT3 lightDir)
{
	XMFLOAT3 eye = Mult(lightDir, -5);
	XMVECTOR eyeV = XMLoadFloat3(&eye);

	XMVECTOR focusV = XMLoadFloat3(&XMFLOAT3_ZERO);

	XMFLOAT3 up = XMFLOAT3(0, 1, 0);
	XMVECTOR upV = XMLoadFloat3(&up);

	ms_viewMatrix = XMMatrixLookAtLH(eyeV, focusV, upV);
	ms_projMatrix = XMMatrixOrthographicLH(SettingsManager::ms_DX12.ShadowMapWidth, SettingsManager::ms_DX12.ShadowMapHeight, -30, 30);
	ms_vpMatrix = ms_viewMatrix * ms_projMatrix;
}

void ShadowManager::Render(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, unordered_map<string, shared_ptr<Batch>>& batchList)
{
	if (ms_currentDSVState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		ResourceManager::TransitionResource(commandList, ms_shadowMapResource.Get(), ms_currentDSVState, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	auto dsvHandle = ms_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	RenderOverride ro = { ms_depthShader.get(), ms_depthRootSig.get() };
	ro.UseShadowMapMat = true;

	for (auto& it : batchList)
	{
		it.second->Render(d3d, commandList, ms_vpMatrix, nullptr, &ro);
		it.second->RenderTrans(d3d, commandList, ms_vpMatrix, nullptr, &ro);
	}	

	ResourceManager::TransitionResource(commandList, ms_shadowMapResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, SettingsManager::ms_DX12.SRVFormat);
	ms_currentDSVState = SettingsManager::ms_DX12.SRVFormat;
}

ID3D12Resource* ShadowManager::GetShadowMap()
{
	return ms_shadowMapResource.Get();
}

XMMATRIX& ShadowManager::GetVPMatrix()
{
	return ms_vpMatrix;
}

void ShadowManager::RenderDebugView(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList)
{
}
