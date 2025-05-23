#include "pch.h"
#include "Scene.h"
#include "ImGUIManager.h"
#include <locale>
#include <codecvt>
#include "Utils.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"
#include "DescriptorManager.h"
#include "AssetFactory.h"
#include "ShadowManager.h"
#include <LoadManager.h>
#include <ShaderComplexityManager.h>
#include <AlkaliGUIManager.h>

shared_ptr<Model> Scene::ms_sphereModel;
shared_ptr<Material> Scene::ms_perFramePBRMat;
shared_ptr<Material> Scene::ms_shadowMapMat;

Scene::Scene(const std::wstring& name, Window* pWindow, bool createDSV)
	: m_Name(name)
	, m_pWindow(pWindow)
	, m_dsvEnabled(createDSV)
	, m_camera(std::make_unique<Camera>(CameraMode::CAMERA_MODE_SCROLL))
	, m_viewMatrix(XMMatrixIdentity())
	, m_projectionMatrix(XMMatrixIdentity())
	, m_viewProjMatrix(XMMatrixIdentity())
	, m_viewProjDebugLinesMatrix(XMMatrixIdentity())
	, m_BackgroundColor(XMFLOAT4(0.4f, 0.6f, 0.9f, 1.0f))
{	
}

Scene::~Scene()
{
}

bool Scene::Init(D3DClass* pD3DClass)
{
	m_d3dClass = pD3DClass;	

	m_scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

	int width = m_pWindow->GetClientWidth();
	int height = m_pWindow->GetClientHeight();
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

#if PACK_COLORS
	m_perFrameCBuffers.DirectionalLight.AmbientColor = PackColor(Mult(XMFLOAT3_ONE, 0.12f));
	m_perFrameCBuffers.DirectionalLight.LightColor = PackColor(XMFLOAT3(1, 1, 1));
#else
	m_perFrameCBuffers.DirectionalLight.AmbientColor = Mult(XMFLOAT3_ONE, 0.12f);
	m_perFrameCBuffers.DirectionalLight.LightColor = XMFLOAT3(1, 1, 1);
#endif
	
	m_perFrameCBuffers.DirectionalLight.LightDirection = Normalize(XMFLOAT3(0.5f, -0.5f, 0.5f));
	m_perFrameCBuffers.DirectionalLight.SpecularPower = 32.0f;

	m_perFrameCBuffers.ShadowMap.NormalBias = 0.2f;
	m_perFrameCBuffers.ShadowMapPixel.Bias = -0.001f;	
	m_perFrameCBuffers.ShadowMapPixel.ShadowWidthPercent = 1.0f / float(MAX_SHADOW_MAP_CASCADES);
	m_perFrameCBuffers.ShadowMapPixel.TexelSize = XMFLOAT2(1.0f / SettingsManager::ms_Misc.ShadowMapResoWidth, 1.0f / SettingsManager::ms_Misc.ShadowMapResoHeight);

	XMFLOAT4 poissonDiscSamples[8] = {
		XMFLOAT4(-0.94201624f, -0.39906216f, 0.94558609f, -0.76890725f),
		XMFLOAT4(-0.094184101f, -0.92938870f, 0.34495938f, 0.29387760f),
		XMFLOAT4(-0.91588581f, 0.45771432f, -0.81544232f, -0.87912464f),
		XMFLOAT4(0.97484398f, 0.75648379f, 0.44323325f, -0.97511554f),
		XMFLOAT4(-0.38277543f, 0.27676845f, 0.53742981f, -0.47373420f),
		XMFLOAT4(-0.26496911f, 0.34436442f, 0.79197514f, 0.19090188f),
		XMFLOAT4(-0.24188840f, 0.99706507f, -0.81409955f, 0.91437590f),
		XMFLOAT4(0.19984126f, 0.78641367f, 0.14383161f, -0.14100790f),
	};

	for (int i = 0; i < POISSON_DISC_COUNT; i++)
		m_perFrameCBuffers.ShadowMapPixel.PoissonDisc[i] = poissonDiscSamples[i];

	if (m_dsvEnabled)
		SetDSVForSize(width, height);

    return true;
}

bool Scene::LoadContent()
{
	AlkaliGUIManager::LogUntaggedMessage("Loading Content for scene");

	shared_ptr<Model> modelPlane;
	{
		CommandQueue* cmdQueueCopy = nullptr;
		cmdQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!cmdQueueCopy)
			throw std::exception("Command Queue Error");

		auto cmdListCopy = cmdQueueCopy->GetAvailableCommandList();

		ms_sphereModel = AssetFactory::CreateModel("Sphere.model", cmdListCopy.Get());
		modelPlane = AssetFactory::CreateModel("Plane.model", cmdListCopy.Get());

		auto fenceValue = cmdQueueCopy->ExecuteCommandList(cmdListCopy);
		cmdQueueCopy->WaitForFenceValue(fenceValue);
	}

	ShaderComplexityManager::Init(modelPlane.get());

	CommandQueue* cmdQueueDirect = nullptr;
	cmdQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!cmdQueueDirect)
		throw std::exception("Command Queue Error");
	auto cmdListDirect = cmdQueueDirect->GetAvailableCommandList();

	m_viewMatrix = m_camera->GetViewMatrix();
	float aspectRatio = SettingsManager::ms_Window.ScreenWidth / SettingsManager::ms_Window.ScreenHeight;
	m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(SettingsManager::ms_Window.FieldOfView), aspectRatio, SettingsManager::ms_Dynamic.NearPlane, SettingsManager::ms_Dynamic.FarPlane);
	m_viewProjMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);
	m_frustum.UpdateValues(m_viewProjMatrix);
	ShadowManager::Init(m_d3dClass, cmdListDirect.Get(), m_frustum);

	if (!ms_perFramePBRMat)
	{
		ms_perFramePBRMat = AssetFactory::CreateMaterial();
		ms_perFramePBRMat->AddCBVs(m_d3dClass, cmdListDirect.Get(), PER_FRAME_PBR_SIZES(), true);
	}

	if (!ms_shadowMapMat)
	{
		ms_shadowMapMat = AssetFactory::CreateMaterial();
		ms_shadowMapMat->AddDynamicSRVs("Shadow Map", 1);

		DXGI_FORMAT format = SettingsManager::ms_Misc.ShadowHDFormatEnabled ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM;
		ms_shadowMapMat->SetDynamicSRV(m_d3dClass, 0, format, ShadowManager::GetShadowMap());
	}

	m_rpiLine.NumCBV_PerDraw = 1;
	m_rpiLine.ParamIndexCBV_PerDraw = 0;

	m_rootSigLine = std::make_shared<RootSig>();
	m_rootSigLine->Init("Line Root Sig", m_rpiLine);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	ShaderArgs args = { L"Line_VS.cso", L"Line_PS.cso", inputLayout, m_rootSigLine->GetRootSigResource() };
	args.Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	args.ForceCompFuncLE = true;
	m_shaderLine = AssetFactory::CreateShader(args);

	{
		vector<UINT> cbvSizesDraw = { sizeof(MatricesLineCB) };

		m_matLine = AssetFactory::CreateMaterial();
		m_matLine->AddCBVs(m_d3dClass, cmdListDirect.Get(), cbvSizesDraw, false);
	}

	{
		vector<DebugLine*> frustumDebugLines;
		for (int i = 0; i < 12; i++)
			frustumDebugLines.push_back(AddDebugLine(XMFLOAT3_ZERO, XMFLOAT3_ZERO, XMFLOAT3_ONE));

		m_frustum.SetDebugLines(frustumDebugLines);

		XMFLOAT3& lDir = m_perFrameCBuffers.DirectionalLight.LightDirection;
		m_debugLineLightDir = AddDebugLine(Mult(lDir, 999), Mult(lDir, -999), XMFLOAT3(1, 1, 0));
	}

	{
		vector<DebugLine*> shadowBoundsDebugLines;
		for (int i = 0; i < MAX_SHADOW_MAP_CASCADES; i++)
		{
			XMFLOAT3 col;
			switch (i)
			{
			case 0:
				col = XMFLOAT3(1, 0, 0);
				break;
			case 1:
				col = XMFLOAT3(0, 1, 0);
				break;
			case 2:
				col = XMFLOAT3(0, 0, 1);
				break;
			case 3:
				col = XMFLOAT3(1, 1, 0);
				break;
			default:
				col = XMFLOAT3(1, 0, 1);
				break;
			}

			col = Mult(col, 2.2f);

			for (int l = 0; l < 12; l++)
			{
				shadowBoundsDebugLines.push_back(AddDebugLine(XMFLOAT3_ZERO, XMFLOAT3_ZERO, col));
			}
		}

		ShadowManager::SetDebugLines(shadowBoundsDebugLines);
	}

	m_viewDepthRPI.NumSRV_Dynamic = 1;
	m_viewDepthRPI.NumCBV_PerDraw = 1;
	m_viewDepthRPI.ParamIndexCBV_PerDraw = 0;
	m_viewDepthRPI.ParamIndexSRV_Dynamic = 1;

	m_viewDepthRootSig = std::make_shared<RootSig>();
	m_viewDepthRootSig->Init("Depth Buffer Root Sig", m_viewDepthRPI, &SettingsManager::ms_DX12.DefaultSamplerDesc, 1);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDepthView =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	ShaderArgs argsDepth = { L"DepthBuffer_VS.cso", L"DepthBuffer_PS.cso", inputLayoutDepthView, m_viewDepthRootSig->GetRootSigResource() };
	argsDepth.CullNone = true;
	argsDepth.DisableDSV = true;
	argsDepth.DisableStencil = true;
	m_viewDepthShader = AssetFactory::CreateShader(argsDepth);

	vector<UINT> viewCBVFrame = { sizeof(DepthViewCB) };

	m_viewDepthMat = AssetFactory::CreateMaterial();
	m_viewDepthMat->AddDynamicSRVs("Depth View", 1);
	m_viewDepthMat->AddCBVs(m_d3dClass, cmdListDirect.Get(), viewCBVFrame, false, "DepthView");

	m_viewDepthMat->SetDynamicSRV(m_d3dClass, 0, DXGI_FORMAT_R32_FLOAT, m_depthBufferResource.Get());

	m_depthViewCB.Resolution = XMFLOAT2(SettingsManager::ms_Window.ScreenWidth, SettingsManager::ms_Window.ScreenHeight);
	m_depthViewCB.MaxValue = 1.0f;
	m_depthViewCB.MinValue = 0.96f;		

	m_viewDepthGO = std::make_unique<GameObject>("Depth Tex", modelPlane, m_viewDepthShader, m_viewDepthMat, true);
	m_viewDepthGO->SetRotation(90, 0, 0);

	if (!m_depthPrepassShader)
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDepth =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		RootParamInfo depthRPI;
		depthRPI.NumCBV_PerDraw = 1;
		depthRPI.ParamIndexCBV_PerDraw = 0;

		m_depthPrepassRootSig = std::make_shared<RootSig>();
		m_depthPrepassRootSig->Init("Depth RS", depthRPI);

		ShaderArgs depthArgs = { L"Depth_VS.hlsl", L"", inputLayoutDepth, m_depthPrepassRootSig->GetRootSigResource() };
		depthArgs.IsDepthShader = true;
		m_depthPrepassShader = AssetFactory::CreateShader(depthArgs, L" - Prepass");

		if (SettingsManager::ms_DX12.DepthAlphaTestEnabled)
		{
			depthRPI.NumSRV = 1;
			depthRPI.ParamIndexSRV = 1;
			m_depthPrepassAlphaTestRS = std::make_shared<RootSig>();
			m_depthPrepassAlphaTestRS->Init("Depth AT RS", depthRPI, &SettingsManager::ms_DX12.DefaultSamplerDesc, 1);

			depthArgs.PS = L"Depth_PS.hlsl";
			depthArgs.NoRTV = true;
			depthArgs.CullNone = true;
			depthArgs.RootSig = m_depthPrepassAlphaTestRS->GetRootSigResource();
			m_depthPrepassAlphaTestShader = AssetFactory::CreateShader(depthArgs, L" - PrepassAT");
		}		
	}

	if (!m_shaderComplexity)
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutComplex =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		RootParamInfo rpiComplex;
		rpiComplex.NumCBV_PerDraw = 2;
		rpiComplex.ParamIndexCBV_PerDraw = 0;

		m_rootSigComplexity = std::make_shared<RootSig>();
		m_rootSigComplexity->Init("Complexity RS", rpiComplex);

		ShaderArgs argsComplex = { L"Shrimple_VS.hlsl", L"Complexity_PS.hlsl", inputLayoutComplex, m_rootSigComplexity->GetRootSigResource() };
		argsComplex.Permutations.push_back("MAX_DEPTH");
		argsComplex.ForceCompFuncLE = true;
		m_shaderComplexity = AssetFactory::CreateShader(argsComplex);
	}

	auto fenceValue = cmdQueueDirect->ExecuteCommandList(cmdListDirect);
	cmdQueueDirect->WaitForFenceValue(fenceValue);

    return true;
}

void Scene::UnloadContent()
{
	AlkaliGUIManager::LogUntaggedMessage("Unloading Scene");

	m_BackgroundColor = XMFLOAT4(0.4f, 0.6f, 0.9f, 1.0f);
	ResourceTracker::ClearBatchList();
	m_camera->Reset();

	m_viewDepthShader.reset();
	m_shaderLine.reset();
	m_matLine.reset();
	m_viewDepthGO.reset();
	m_viewDepthMat.reset();
	m_viewDepthShader.reset();
	m_viewDepthRootSig.reset();
	m_rootSigLine.reset();
	m_skyboxTex.reset();
	m_debugLineList.clear();
	m_depthPrepassRootSig.reset();
	m_depthPrepassShader.reset();

	if (ms_sphereModel)
	{
		ms_sphereModel.reset();
	}

	AlkaliGUIManager::LogUntaggedMessage("Unloading Scene Finished");
}

void Scene::Destroy()
{
}

void Scene::OnUpdate(TimeEventArgs& e)
{
	if (!ImGUIManager::MouseHoveringImGui())
	{
		m_camera->Update(e);
		m_perFrameCBuffers.Camera.CameraPosition = m_camera->GetWorldPosition();
	}

	LoadManager::ExecuteCPUWaitingLists();

	if (SettingsManager::ms_Window.DebugFlashingBackgroundColors)
		m_BackgroundColor = XMFLOAT4(Rand01(), Rand01(), Rand01(), Rand01());

	m_viewMatrix = m_camera->GetViewMatrix();

	float aspectRatio = SettingsManager::ms_Window.ScreenWidth / SettingsManager::ms_Window.ScreenHeight;
	m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(SettingsManager::ms_Window.FieldOfView), aspectRatio, SettingsManager::ms_Dynamic.NearPlane, SettingsManager::ms_Dynamic.FarPlane);
	m_viewProjMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);

	XMMATRIX debugLineProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(SettingsManager::ms_Window.FieldOfView), aspectRatio, SettingsManager::ms_Dynamic.NearPlane, SettingsManager::ms_Misc.DebugLinesFarPlane);
	m_viewProjDebugLinesMatrix = XMMatrixMultiply(m_viewMatrix, debugLineProj);

	if (!SettingsManager::ms_Dynamic.FreezeFrustum)
		m_frustum.UpdateValues(m_viewProjMatrix);
	
	bool showLines = SettingsManager::ms_Dynamic.AlwaysShowFrustumDebugLines || SettingsManager::ms_Dynamic.FreezeFrustum;
	if (showLines)
		m_frustum.CalculateDebugLinePoints(m_d3dClass);

	m_frustum.SetDebugLinesEnabled(showLines);

	XMFLOAT3& lDir = m_perFrameCBuffers.DirectionalLight.LightDirection;
	if (m_debugLineLightDir)
		m_debugLineLightDir->SetPositions(m_d3dClass, Mult(lDir, 999), Mult(lDir, -999));

	if (m_skyboxTex)
		m_perFrameCBuffers.EnvMap.EnvMapMipLevels = m_skyboxTex->GetMipLevels();

	float screenWidth = static_cast<float>(m_pWindow->GetClientWidth());
	float screenHeight = static_cast<float>(m_pWindow->GetClientHeight());
	m_depthViewCB.Resolution = XMFLOAT2(screenWidth, screenHeight);

	if (InputManager::IsKeyDown(KeyCode::G))
		SettingsManager::ms_Dynamic.GUIEnabled = !SettingsManager::ms_Dynamic.GUIEnabled;

	if (m_shadowMapCounter >= SettingsManager::ms_Dynamic.Shadow.TimeSlice && SettingsManager::ms_Dynamic.Shadow.Enabled)
	{
		m_perFrameCBuffers.ShadowMap.CascadeCount = static_cast<float>(SettingsManager::ms_Dynamic.Shadow.CascadeCount);
		m_perFrameCBuffers.ShadowMapPixel.CascadeCount = static_cast<float>(SettingsManager::ms_Dynamic.Shadow.CascadeCount);
		m_perFrameCBuffers.ShadowMapPixel.PCFSampleCount = static_cast<float>(SettingsManager::ms_Dynamic.Shadow.PCFSampleCount);
		m_perFrameCBuffers.ShadowMapPixel.PCFSampleRange = ShadowManager::GetPCFSampleRange(SettingsManager::ms_Dynamic.Shadow.PCFSampleCount);
		m_perFrameCBuffers.ShadowMapPixel.CascadeDistances = ShadowManager::GetCascadeDistances();

		ShadowManager::Update(m_d3dClass, lDir, m_frustum, m_camera->GetWorldPosition());

		auto vps = ShadowManager::GetVPMatrices();
		for (int i = 0; i < SettingsManager::ms_Dynamic.Shadow.CascadeCount; i++)
			m_perFrameCBuffers.ShadowMap.ShadowMatrix[i] = vps[i];
	}	

	if (SettingsManager::ms_Dynamic.BatchSortingEnabled)
	{
		auto& batchList = ResourceTracker::GetBatches();
		for (auto& it : batchList)
			it.second->SortObjects(m_camera->GetTransform().Position);
	}

	m_updated = true;
}

void Scene::OnRender(TimeEventArgs& e)
{
	if (!m_updated)
		return;

	if (SettingsManager::ms_DX12.DebugRenderSingleFrameOnly && m_debugRenderedFrame)
		return;

	const UINT64 COLOR_BLACK = PIX_COLOR(0, 0, 0);
	const UINT64 COLOR_GREY = PIX_COLOR(28, 91, 90);
	const UINT64 COLOR_CYAN = PIX_COLOR(74, 217, 255);
	const UINT64 COLOR_LIME = PIX_COLOR(209, 255, 161);
	const UINT64 COLOR_GREEN = PIX_COLOR(94, 191, 114);
	const UINT64 COLOR_DARKGREEN = PIX_COLOR(30, 123, 32);
	const UINT64 COLOR_PURPLE = PIX_COLOR(101, 56, 172);
	const UINT64 COLOR_PINK = PIX_COLOR(185, 158, 235);

	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtvHandle = m_pWindow->GetCurrentRenderTargetView();
	auto dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	int backBufferIndex = m_pWindow->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_STATES backBufferCurState = D3D12_RESOURCE_STATE_PRESENT;

	if (SettingsManager::ms_Dynamic.ShaderComplexityMode)
	{		
		ShaderComplexityManager::CalculateComplexityTable(m_d3dClass, backBufferIndex, m_viewport, m_scissorRect, rtvHandle, dsvHandle, backBuffer);
		backBufferCurState = D3D12_RESOURCE_STATE_RENDER_TARGET;		
	}	

	CommandQueue* cmdQueue = nullptr;
	cmdQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto cmdList = cmdQueue->GetAvailableCommandList();

	auto& batchList = ResourceTracker::GetBatches();

	auto globalHeap = DescriptorManager::GetHeap();
	ID3D12DescriptorHeap* ppHeaps[] = { globalHeap };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	if (SettingsManager::ms_Dynamic.Shadow.MultiViewportEnabled)
	{
		D3D12_RECT rects[4] = { m_scissorRect, m_scissorRect , m_scissorRect , m_scissorRect };
		cmdList->RSSetScissorRects(SettingsManager::ms_Dynamic.Shadow.CascadeCount, rects);
	}
	else
		cmdList->RSSetScissorRects(1, &m_scissorRect);

	if (m_shadowMapCounter >= SettingsManager::ms_Dynamic.Shadow.TimeSlice && SettingsManager::ms_Dynamic.Shadow.Enabled)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_BLACK, "Shadow Pass");
		ShadowManager::Render(m_d3dClass, cmdList.Get(), batchList, m_frustum, backBufferIndex);
		PIXEndEvent(cmdList.Get());

		m_shadowMapCounter = 0;
	}
	else
		m_shadowMapCounter++;	

	cmdList->RSSetViewports(1, &m_viewport);

	if (backBufferCurState != D3D12_RESOURCE_STATE_RENDER_TARGET)
		ResourceManager::TransitionResource(cmdList.Get(), backBuffer, backBufferCurState, D3D12_RESOURCE_STATE_RENDER_TARGET);

	ClearBackBuffer(cmdList.Get());	

	if (ms_perFramePBRMat)
	{
		ms_perFramePBRMat->SetCBV_PerFrame(0, &m_perFrameCBuffers.Camera, sizeof(CameraCB), backBufferIndex);
		ms_perFramePBRMat->SetCBV_PerFrame(1, &m_perFrameCBuffers.DirectionalLight, sizeof(DirectionalLightCB), backBufferIndex);
		ms_perFramePBRMat->SetCBV_PerFrame(2, &m_perFrameCBuffers.ShadowMap, sizeof(ShadowMapCB), backBufferIndex);
		ms_perFramePBRMat->SetCBV_PerFrame(3, &m_perFrameCBuffers.ShadowMapPixel.CascadeDistances, sizeof(ShadowMapPixelCB), backBufferIndex);
		ms_perFramePBRMat->SetCBV_PerFrame(4, &m_perFrameCBuffers.EnvMap, sizeof(EnvMapCB), backBufferIndex);
	}

	if (m_viewDepthMat)
		m_viewDepthMat->SetCBV_PerDraw(0, &m_depthViewCB, sizeof(DepthViewCB), backBufferIndex);

	BatchArgs batchArgs = { m_viewMatrix, m_projectionMatrix };
	batchArgs.BackBufferIndex = backBufferIndex;
	batchArgs.pFrustum = &m_frustum;

	if (SettingsManager::ms_Dynamic.DepthPrePassEnabled)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_BLACK, "Depth Pre-Pass");
		cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

		RenderOverride ro = { m_depthPrepassShader.get(), m_depthPrepassRootSig.get() };
		ro.UseDepthMaterial = true;
		ro.DepthMatIndex = MAX_SHADOW_MAP_CASCADES;			
		batchArgs.pOverride = &ro;

		if (SettingsManager::ms_Dynamic.BoundingSphereMode)
		{
			ro.ModelOverride = ms_sphereModel.get();
			ro.ModifyTransformToCentroid = true;
		}

		Shader* lastSetShader = nullptr;
		batchArgs.ppLastUsedShader = &lastSetShader;
		for (auto& it : batchList)
		{
			it.second->Render(m_d3dClass, cmdList.Get(), batchArgs);

			if (SettingsManager::ms_DX12.DepthAlphaTestEnabled)
			{
				ro.ShaderOverride = m_depthPrepassAlphaTestShader.get();
				ro.RootSigOverride = m_depthPrepassAlphaTestRS.get();
				ro.AddSRVToDepthMat = true;
				it.second->RenderAT(m_d3dClass, cmdList.Get(), batchArgs);
			}				
		}

		batchArgs.pOverride = nullptr;
		batchArgs.ppLastUsedShader = nullptr;
		PIXEndEvent(cmdList.Get());		
	}
	
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);	

	bool requireCPUGPUSync = false;
	batchArgs.pRequireCPUGPUSync = &requireCPUGPUSync;

	RenderOverride ro;
	if (SettingsManager::ms_Dynamic.BoundingSphereMode)
	{
		batchArgs.pOverride = &ro;
		batchArgs.pOverride->ModelOverride = ms_sphereModel.get();
		batchArgs.pOverride->ModifyTransformToCentroid = true;
	}
	else if (SettingsManager::ms_Dynamic.ShaderComplexityMode)
	{
		batchArgs.pOverride = &ro;
		batchArgs.pOverride->ShaderOverride = m_shaderComplexity.get();
		batchArgs.pOverride->RootSigOverride = m_rootSigComplexity.get();
		batchArgs.pOverride->UseComplexityMaterial = true;
	}

	PIXBeginEvent(cmdList.Get(), COLOR_GREEN, "Opaque Pass");	
	for (auto& it : batchList)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_GREEN, it.second->m_Name.c_str());
		it.second->Render(m_d3dClass, cmdList.Get(), batchArgs);
		PIXEndEvent(cmdList.Get());
	}		
	PIXEndEvent(cmdList.Get());

	if (SettingsManager::ms_Dynamic.ATGoEnabled)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_DARKGREEN, "Alpha Test Pass");
		for (auto& it : batchList)
		{
			PIXBeginEvent(cmdList.Get(), COLOR_DARKGREEN, it.second->m_Name.c_str());
			it.second->RenderAT(m_d3dClass, cmdList.Get(), batchArgs);
			PIXEndEvent(cmdList.Get());
		}
		PIXEndEvent(cmdList.Get());
	}

	if (SettingsManager::ms_Dynamic.DebugLinesEnabled)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_CYAN, "Line Pass");
		RenderDebugLines(cmdList.Get(), rtvHandle, dsvHandle, backBufferIndex);
		PIXEndEvent(cmdList.Get());
	}	

	if (SettingsManager::ms_Dynamic.TransparentGOEnabled)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_LIME, "Trans Pass");
		for (auto& it : batchList)
		{
			PIXBeginEvent(cmdList.Get(), COLOR_LIME, it.second->m_Name.c_str());
			it.second->RenderTrans(m_d3dClass, cmdList.Get(), batchArgs);
			PIXEndEvent(cmdList.Get());
		}
		PIXEndEvent(cmdList.Get());
	}	

	if (SettingsManager::ms_Dynamic.VisualiseDSVEnabled && m_viewDepthGO)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_GREY, "Visualise DSV Pass");

		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		cmdList->SetGraphicsRootSignature(m_viewDepthRootSig->GetRootSigResource());
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ResourceManager::TransitionResource(cmdList.Get(), m_depthBufferResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);		

		m_viewDepthGO->Render(m_d3dClass, cmdList.Get(), m_viewDepthRPI, backBufferIndex);

		ResourceManager::TransitionResource(cmdList.Get(), m_depthBufferResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		
		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		PIXEndEvent(cmdList.Get());
	}
	
	if (SettingsManager::ms_Dynamic.Shadow.Enabled && SettingsManager::ms_Dynamic.VisualiseShadowMap)
	{
		float screenWidth = static_cast<float>(m_pWindow->GetClientWidth());
		float screenHeight = static_cast<float>(m_pWindow->GetClientHeight());
		const XMFLOAT2 screenReso = XMFLOAT2(screenWidth, screenHeight);

		PIXBeginEvent(cmdList.Get(), COLOR_GREY, "Visualise Shadow Map Pass");
		ShadowManager::RenderDebugView(m_d3dClass, cmdList.Get(), rtvHandle, dsvHandle, backBufferIndex, screenReso);
		PIXEndEvent(cmdList.Get());
	}

	if (SettingsManager::ms_Dynamic.GUIEnabled)
	{
		PIXBeginEvent(cmdList.Get(), COLOR_PURPLE, "GUI Pass");
		ImGUIManager::Render(cmdList.Get());
		PIXEndEvent(cmdList.Get());
	}	

	UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
	Present(cmdList.Get(), cmdQueue); 

	if (SettingsManager::ms_Dynamic.ForceSyncCPUGPU || requireCPUGPUSync)
		cmdQueue->WaitForFenceValue(m_FenceValues.at(currentBackBufferIndex));

	m_debugRenderedFrame = true;
}

void Scene::OnResize(ResizeEventArgs& e)
{
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

	if (m_dsvEnabled)
		SetDSVForSize(e.Width, e.Height);
}

void Scene::OnWindowDestroy()
{
}

void Scene::StaticShutdown()
{
	ms_perFramePBRMat.reset();
	ms_sphereModel.reset();
}

Window* Scene::GetWindow()
{
	return m_pWindow;
}

PerFrameCBuffers_PBR& Scene::GetPerFrameCBuffers()
{
	return m_perFrameCBuffers;
}

Camera* Scene::GetCamera()
{
	return m_camera.get();
}

vector<float>& Scene::GetPresentWaitTimes()
{
	return m_presentWaitTimes;
}

void Scene::ClearBackBuffer(ID3D12GraphicsCommandList2* cmdList)
{
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();	

	cmdList->ClearRenderTargetView(rtv, reinterpret_cast<FLOAT*>(&m_BackgroundColor), 0, nullptr);
	ClearDepth(cmdList, dsv);
}

void Scene::ClearDepth(ID3D12GraphicsCommandList2* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void Scene::Present(ID3D12GraphicsCommandList2* cmdList, CommandQueue* cmdQueue)
{
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(cmdList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	m_FenceValues.at(currentBackBufferIndex) = cmdQueue->ExecuteCommandList(cmdList);

	UINT nextBackBufferIndex = m_pWindow->Present();

	if (SettingsManager::ms_Dynamic.DebugFramePresentInfo)
	{
		OutputDebugString((L"Presented frame finished on fence " + std::to_wstring(m_FenceValues.at(currentBackBufferIndex)) + L"\n").c_str());
		OutputDebugString((L"Waiting for fence " + std::to_wstring(m_FenceValues.at(nextBackBufferIndex)) + L"\n").c_str());
	}
	auto startTimeProfiler = std::chrono::high_resolution_clock::now();

	cmdQueue->WaitForFenceValue(m_FenceValues.at(nextBackBufferIndex));
	
	std::chrono::duration<double, std::milli> timeTaken = std::chrono::high_resolution_clock::now() - startTimeProfiler;

	m_presentWaitCounter++;
	m_averagingWaitTimes.push_back(timeTaken.count());
	if (m_presentWaitCounter > 30)
	{
		m_presentWaitCounter = 0;

		float average = 0;
		for (int i = 0; i < m_averagingWaitTimes.size(); i++)
			average += m_averagingWaitTimes[i];
		average /= m_averagingWaitTimes.size();
		m_averagingWaitTimes.clear();

		m_presentWaitTimes.push_back(average);
		if (m_presentWaitTimes.size() > 1000)
			m_presentWaitTimes.erase(m_presentWaitTimes.begin());
	}	

	if (SettingsManager::ms_Dynamic.DebugFramePresentInfo)
	{		
		wstring output = L"Time spent waiting for fence was " + std::to_wstring(timeTaken.count()) + L" ms\n";
		OutputDebugString(output.c_str());		
	}	
}

void Scene::SetDSVForSize(int width, int height)
{
	if (!m_dsvEnabled)
		return;

	AlkaliGUIManager::LogUntaggedMessage("Setting DSV Size");

	HRESULT hr;

	auto device = m_d3dClass->GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
	ThrowIfFailed(hr);

	m_d3dClass->Flush();

	width = std::max(1, width);
	height = std::max(1, height);

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = SettingsManager::ms_DX12.DSVFormat;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto dsvResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(optimizedClearValue.Format, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);	

	hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &dsvResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&m_depthBufferResource));
	ThrowIfFailed(hr);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = optimizedClearValue.Format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE heapStartHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(m_depthBufferResource.Get(), &dsvDesc, heapStartHandle);	

	if (m_viewDepthMat)
	{
		m_viewDepthMat->SetDynamicSRV(m_d3dClass, 0, DXGI_FORMAT_R32_FLOAT, m_depthBufferResource.Get());
	}

	AlkaliGUIManager::LogUntaggedMessage("Setting DSV Finished");
}

void Scene::SetDSVFlags(D3D12_DSV_FLAGS flags) 
{
	auto device = m_d3dClass->GetDevice();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = SettingsManager::ms_DX12.DSVFormat;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = flags;

	D3D12_CPU_DESCRIPTOR_HANDLE heapStartHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(m_depthBufferResource.Get(), &dsv, heapStartHandle);
}

DebugLine* Scene::AddDebugLine(XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color)
{
	auto line = std::make_shared<DebugLine>(m_d3dClass, m_rpiLine, m_shaderLine, m_matLine, start, end, color);
	m_debugLineList.push_back(line);
	return line.get();
}

void Scene::RenderDebugLines(ID3D12GraphicsCommandList2* cmdListDirect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, const int& backBufferIndex)
{
	Shader* lastSetShader = nullptr;
	for (int i = 0; i < m_debugLineList.size(); i++)
	{
		m_debugLineList.at(i)->Render(cmdListDirect, m_rootSigLine->GetRootSigResource(), m_viewport, m_scissorRect, rtv, dsv, m_viewProjDebugLinesMatrix, backBufferIndex, &lastSetShader);
	}
}
