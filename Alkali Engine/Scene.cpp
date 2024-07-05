#include "pch.h"
#include "Scene.h"
#include "ImGUIManager.h"
#include <locale>
#include <codecvt>
#include "Utils.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"
#include "DescriptorManager.h"

shared_ptr<Model> Scene::ms_sphereModel;
bool Scene::ms_sphereMode;
bool Scene::ms_visualizeDSV;
bool Scene::ms_sortBatchGos;
bool Scene::ms_forceReloadBinTex;
bool Scene::ms_mipMapDebugMode;
bool Scene::ms_renderDebugLines;
shared_ptr<Material> Scene::ms_perFramePBRMat;

Scene::Scene(const std::wstring& name, Window* pWindow, bool createDSV)
	: m_Name(name)
	, m_pWindow(pWindow)
	, m_dsvEnabled(createDSV)
	, m_camera(std::make_unique<Camera>(CameraMode::CAMERA_MODE_SCROLL))
	, m_viewMatrix(XMMatrixIdentity())
	, m_projectionMatrix(XMMatrixIdentity())
	, m_viewProjMatrix(XMMatrixIdentity())
	, m_FoV(45.0f)
{
	SetBackgroundColor(0.4f, 0.6f, 0.9f, 1.0f);
	ms_sortBatchGos = true;
	ms_renderDebugLines = false; // DISABLED TEMPORARILY
}

Scene::~Scene()
{
}

bool Scene::Init(D3DClass* pD3DClass)
{
	m_d3dClass = pD3DClass;	

	m_scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

	float width = m_pWindow->GetClientWidth();
	float height = m_pWindow->GetClientHeight();
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

	m_perFrameCBuffers.DirectionalLight.AmbientColor = XMFLOAT3(0.15f, 0.15f, 0.15f);
	m_perFrameCBuffers.DirectionalLight.LightDiffuse = XMFLOAT3(1, 1, 1);
	m_perFrameCBuffers.DirectionalLight.LightDirection = Normalize(XMFLOAT3(0.5f, -0.5f, 0.5f));
	m_perFrameCBuffers.DirectionalLight.SpecularPower = 32.0f;

	DescriptorManager::Init(m_d3dClass, DESCRIPTOR_HEAP_SIZE);

	if (m_dsvEnabled)
		SetDSVForSize(width, height);

    return true;
}

bool Scene::LoadContent()
{
	shared_ptr<Model> modelPlane;
	{
		CommandQueue* commandQueueCopy = nullptr;
		commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		if (!ResourceTracker::TryGetModel("Sphere.model", ms_sphereModel))
		{
			ms_sphereModel->Init(commandListCopy.Get(), L"Sphere.model");
		}
		
		if (!ResourceTracker::TryGetModel("Plane.model", modelPlane))
		{
			modelPlane->Init(commandListCopy.Get(), L"Plane.model");
		}

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");
	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	if (!ms_perFramePBRMat)
	{
		vector<UINT> cbvSizesFrame = { sizeof(CameraCB), sizeof(DirectionalLightCB) };
		ms_perFramePBRMat = std::make_shared<Material>();
		ms_perFramePBRMat->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesFrame, true);
		ResourceTracker::AddMaterial(ms_perFramePBRMat);
	}

	m_rpiLine.NumCBV_PerFrame = 0;
	m_rpiLine.NumCBV_PerDraw = 1;
	m_rpiLine.NumSRV = 0;
	m_rpiLine.ParamIndexCBV_PerDraw = 0;
	m_rpiLine.ParamIndexCBV_PerFrame = -1;
	m_rpiLine.ParamIndexSRV = -1;

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, m_rpiLine.NumCBV_PerDraw, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[m_rpiLine.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC sampler[1];
		sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].MipLODBias = 0;
		sampler[0].MaxAnisotropy = 0;
		sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler[0].MinLOD = 0.0f;
		sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
		sampler[0].ShaderRegister = 0;
		sampler[0].RegisterSpace = 0;
		sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		m_rootSigLine = ResourceManager::CreateRootSignature(rootParameters, _countof(rootParameters), sampler, 1);
		m_rootSigLine->SetName(L"Line Root Sig");
	}

	if (!ResourceTracker::TryGetShader("Line_VS.cso - Line_PS.cso", m_shaderLine))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_shaderLine->InitPreCompiled(L"Line_VS.cso", L"Line_PS.cso", inputLayout, m_rootSigLine.Get(), m_d3dClass->GetDevice(), false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	}

	{
		vector<UINT> cbvSizesDraw = { sizeof(MatricesLineCB) };

		m_matLine = std::make_shared<Material>();
		m_matLine->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
	}

	{
		vector<DebugLine*> frustumDebugLines;
		for (int i = 0; i < 12; i++)
			frustumDebugLines.push_back(AddDebugLine(XMFLOAT3_ZERO, XMFLOAT3_ZERO, XMFLOAT3_ONE));

		m_frustum.SetDebugLines(frustumDebugLines);

		XMFLOAT3& lDir = m_perFrameCBuffers.DirectionalLight.LightDirection;
		m_debugLineLightDir = AddDebugLine(Mult(lDir, 999), Mult(lDir, -999), XMFLOAT3(1, 1, 0));
	}

	m_rpiDepth.NumCBV_PerFrame = 0;
	m_rpiDepth.NumCBV_PerDraw = 0;
	m_rpiDepth.NumSRV = 1;
	m_rpiDepth.ParamIndexCBV_PerDraw = -1;
	m_rpiDepth.ParamIndexCBV_PerFrame = -1;
	m_rpiDepth.ParamIndexSRV = 0;

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_rpiDepth.NumSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[m_rpiDepth.ParamIndexSRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC sampler[1];
		sampler[0].Filter = DEFAULT_SAMPLER_FILTER;
		sampler[0].AddressU = DEFAULT_SAMPLER_ADDRESS_MODE;
		sampler[0].AddressV = DEFAULT_SAMPLER_ADDRESS_MODE;
		sampler[0].AddressW = DEFAULT_SAMPLER_ADDRESS_MODE;
		sampler[0].MipLODBias = 0;
		sampler[0].MaxAnisotropy = 0;
		sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler[0].MinLOD = 0.0f;
		sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
		sampler[0].ShaderRegister = 0;
		sampler[0].RegisterSpace = 0;
		sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		m_rootSigDepth = ResourceManager::CreateRootSignature(rootParameters, _countof(rootParameters), sampler, 1);
		m_rootSigDepth->SetName(L"Depth Buffer Tex Root Sig");
	}

	if (!ResourceTracker::TryGetShader("Depth_VS.cso - Depth_PS.cso", m_shaderDepth))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_shaderDepth->InitPreCompiled(L"Depth_VS.cso", L"Depth_PS.cso", inputLayout, m_rootSigDepth.Get(), m_d3dClass->GetDevice(), true, true);
	}

	m_depthBufferMat = std::make_shared<Material>();
	m_depthBufferMat->AddDynamicSRVs(1);

	m_goDepthTex = std::make_unique<GameObject>("Depth Tex", m_rpiDepth, modelPlane, m_shaderDepth, m_depthBufferMat, true);
	m_goDepthTex->SetRotation(90, 0, 0);

	auto fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

    return true;
}

void Scene::UnloadContent()
{
	SetBackgroundColor(0.4f, 0.6f, 0.9f, 1.0f);
	ResourceTracker::ClearBatchList();
	m_camera->Reset();

	if (ms_sphereModel)
	{
		ms_sphereModel.reset();
	}
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

	m_viewMatrix = m_camera->GetViewMatrix();
	m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
	m_viewProjMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);

	if (!m_freezeFrustum)
		m_frustum.UpdateValues(m_viewProjMatrix);
	
	bool showLines = PERMA_FRUSTUM_DEBUG_LINES || m_freezeFrustum;
	if (showLines)
		m_frustum.CalculateDebugLinePoints(m_d3dClass);

	m_frustum.SetDebugLinesEnabled(showLines);

	XMFLOAT3& lDir = m_perFrameCBuffers.DirectionalLight.LightDirection;
	m_debugLineLightDir->SetPositions(m_d3dClass, Mult(lDir, 999), Mult(lDir, -999));

	if (ms_sortBatchGos)
	{
		auto& batchList = ResourceTracker::GetBatches();
		for (auto& it : batchList)
			it.second->SortObjects(m_camera->GetTransform().Position);
	}
}

void Scene::OnRender(TimeEventArgs& e)
{
	ms_perFramePBRMat->SetCBV_PerFrame(0, &m_perFrameCBuffers.Camera, sizeof(CameraCB));
	ms_perFramePBRMat->SetCBV_PerFrame(1, &m_perFrameCBuffers.DirectionalLight, sizeof(DirectionalLightCB));

	RenderImGui();

	CommandQueue* commandQueue = nullptr;
	commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueue)
		return;

	auto commandList = commandQueue->GetAvailableCommandList();

	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtvCPUDesc = m_pWindow->GetCurrentRenderTargetView();
	auto dsvCPUDesc = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ClearBackBuffer(commandList.Get());

	commandList->RSSetViewports(1, &m_viewport);
	commandList->RSSetScissorRects(1, &m_scissorRect);

	auto globalHeap = DescriptorManager::GetHeap();
	ID3D12DescriptorHeap* ppHeaps[] = { globalHeap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	UINT numRenderTargets = 1;
	commandList->OMSetRenderTargets(numRenderTargets, &rtvCPUDesc, FALSE, &dsvCPUDesc);

	auto& batchList = ResourceTracker::GetBatches();
	for (auto& it : batchList)
		it.second->Render(commandList.Get(), m_viewProjMatrix, m_frustum);

	for (auto& it : batchList)
		it.second->RenderTrans(commandList.Get(), m_viewProjMatrix, m_frustum);

	if (ms_renderDebugLines)
		RenderDebugLines(commandList.Get(), rtvCPUDesc, dsvCPUDesc);

	if (ms_visualizeDSV && m_goDepthTex)
	{
		commandList->OMSetRenderTargets(numRenderTargets, &rtvCPUDesc, FALSE, nullptr); // Disable DSV

		commandList->SetGraphicsRootSignature(m_rootSigDepth.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Convert DSV to SRV and assign as a texture to be read in the shader
		ResourceManager::TransitionResource(commandList.Get(), m_depthBufferResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_depthBufferMat->SetDynamicSRV(m_d3dClass, 0, DXGI_FORMAT_R32_FLOAT, m_depthBufferResource.Get());

		m_depthBufferMat->AssignMaterial(commandList.Get(), m_rpiDepth);

		m_goDepthTex->Render(commandList.Get());
		
		commandList->OMSetRenderTargets(numRenderTargets, &rtvCPUDesc, FALSE, &dsvCPUDesc);
	}

	ImGUIManager::Render(commandList.Get());

	UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
	Present(commandList.Get(), commandQueue); 
	
	// Forced to wait for execution of current back buffer command list so we can transition the depth buffer back
	if (ms_visualizeDSV && m_goDepthTex)
	{
		commandQueue->WaitForFenceValue(m_FenceValues.at(currentBackBufferIndex)); 

		auto commandList = commandQueue->GetAvailableCommandList();
		ResourceManager::TransitionResource(commandList.Get(), m_depthBufferResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandQueue->ExecuteCommandList(commandList);
	}
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

void Scene::InstantiateCubes(int count)
{
	shared_ptr<Model> model;
	if (!ResourceTracker::TryGetModel("Cube", model))
	{
		CommandQueue* commandQueueCopy = nullptr;
	    commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		model->Init(commandListCopy.Get(), L"Cube.model");

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");

	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	shared_ptr<Texture> texture;
	shared_ptr<Texture> normalMap;
	if (!ResourceTracker::TryGetTexture("Baba.png", texture))
	{
		texture->Init(m_d3dClass, commandListDirect.Get(), "Baba.png");
	}

	if (!ResourceTracker::TryGetTexture("Bistro/Pavement_Cobblestone_Big_BLENDSHADER_Normal.dds", normalMap))
	{
		normalMap->Init(m_d3dClass, commandListDirect.Get(), "Bistro/Pavement_Cobblestone_Big_BLENDSHADER_Normal.dds");
	}

	vector<UINT> cbvSizes = { sizeof(MatricesCB) };
	vector<shared_ptr<Texture>> textures = { texture, normalMap };

	shared_ptr<Material> material = std::make_shared<Material>();
	material->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizes, false);
	material->AddSRVs(m_d3dClass, textures);

	RootParamInfo rootParamInfo;
	rootParamInfo.NumCBV_PerFrame = 0;
	rootParamInfo.NumCBV_PerDraw = 1;
	rootParamInfo.NumSRV = 2;
	rootParamInfo.ParamIndexCBV_PerDraw = 0;
	rootParamInfo.ParamIndexCBV_PerFrame = -1;
	rootParamInfo.ParamIndexSRV = 1;

	ComPtr<ID3D12RootSignature> rootSigPBR;
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rootParamInfo.NumSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rootParamInfo.NumCBV_PerDraw, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		const int paramCount = 2;
		CD3DX12_ROOT_PARAMETER1 rootParameters[paramCount];
		rootParameters[rootParamInfo.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[rootParamInfo.ParamIndexSRV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC sampler[1];
		sampler[0].Filter = DEFAULT_SAMPLER_FILTER;
		sampler[0].AddressU = DEFAULT_SAMPLER_ADDRESS_MODE;
		sampler[0].AddressV = DEFAULT_SAMPLER_ADDRESS_MODE;
		sampler[0].AddressW = DEFAULT_SAMPLER_ADDRESS_MODE;
		sampler[0].MipLODBias = 0;
		sampler[0].MaxAnisotropy = DEFAULT_SAMPLER_MAX_ANISOTROPIC;
		sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler[0].MinLOD = 0.0f;
		sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
		sampler[0].ShaderRegister = 0;
		sampler[0].RegisterSpace = 0;
		sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootSigPBR = ResourceManager::CreateRootSignature(rootParameters, paramCount, sampler, 1);
		rootSigPBR->SetName(L"Tutorial2 Root Sig");
	}

	shared_ptr<Shader> shader;
	if (!ResourceTracker::TryGetShader("PBR.vs - PBR.ps", shader))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		
		shader->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSigPBR.Get(), m_d3dClass->GetDevice());
	}

	shared_ptr<Batch> batch;
	if (!ResourceTracker::TryGetBatch("PBR Basic", batch))
	{
		batch->Init("PBR Basic", rootSigPBR);
	}

	for (int i = 0; i < count; i++)
	{
		string id = "Cube_" + std::to_string(i);
		auto go = GameObject(id, rootParamInfo, model, shader, material);
		batch->AddGameObject(go);
	}
}

bool Scene::IsSphereModeOn(Model** model)
{
	if (!ms_sphereMode)
		return false;

	*model = ms_sphereModel.get();
	return true;
}

bool Scene::IsForceReloadBinTex()
{
	return ms_forceReloadBinTex;
}

bool Scene::IsMipMapDebugMode()
{
	return ms_mipMapDebugMode;
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

void Scene::SetBackgroundColor(float r, float g, float b, float a)
{
	m_backgroundColor[0] = r;
	m_backgroundColor[1] = g;
	m_backgroundColor[2] = b;
	m_backgroundColor[3] = a;
}

void Scene::ClearBackBuffer(ID3D12GraphicsCommandList2* commandList)
{
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->ClearRenderTargetView(rtv, m_backgroundColor, 0, nullptr);
	ClearDepth(commandList, dsv);
}

void Scene::ClearDepth(ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void Scene::Present(ID3D12GraphicsCommandList2* commandList, CommandQueue* commandQueue)
{
	UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	m_FenceValues.at(currentBackBufferIndex) = commandQueue->ExecuteCommandList(commandList);

	UINT nextBackBufferIndex = m_pWindow->Present();

	commandQueue->WaitForFenceValue(m_FenceValues.at(nextBackBufferIndex));
}

void Scene::SetDSVForSize(int width, int height)
{
	if (!m_dsvEnabled)
		return;

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
	optimizedClearValue.Format = DSV_FORMAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto dsvResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DSV_FORMAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &dsvResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&m_depthBufferResource));
	ThrowIfFailed(hr);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DSV_FORMAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE heapStartHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(m_depthBufferResource.Get(), &dsv, heapStartHandle);
}

void Scene::SetDSVFlags(D3D12_DSV_FLAGS flags) 
{
	auto device = m_d3dClass->GetDevice();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DSV_FORMAT;
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

void Scene::RenderDebugLines(ID3D12GraphicsCommandList2* commandListDirect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	for (int i = 0; i < m_debugLineList.size(); i++)
	{
		m_debugLineList.at(i)->Render(commandListDirect, m_rootSigLine.Get(), m_viewport, m_scissorRect, rtv, dsv, m_viewProjMatrix);
	}
}

void Scene::RenderImGui() 
{
	auto& shaderList = ResourceTracker::GetShaders();

	if (ImGui::CollapsingHeader("Settings"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		if (ImGui::TreeNode("Engine##2"))
		{
			ImGui::SeparatorText("DX12");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				bool prevWireFrame = Shader::ms_GlobalFillWireframeMode;
				ImGui::Checkbox("Wireframe", &Shader::ms_GlobalFillWireframeMode);

				bool prevBackCull = Shader::ms_GlobalCullNone;
				ImGui::Checkbox("Don't Cull Backfaces", &Shader::ms_GlobalCullNone);

				if (prevWireFrame != Shader::ms_GlobalFillWireframeMode || prevBackCull != Shader::ms_GlobalCullNone)
				{
					for (auto& it : shaderList)
					{
						it.second->Recompile(m_d3dClass->GetDevice());
					}
				}

				ImGui::Checkbox("Freeze Frustum Culling", &m_freezeFrustum);

				ImGui::Checkbox("Show Bounding Spheres", &ms_sphereMode);

				ImGui::Checkbox("Show DSV", &ms_visualizeDSV);

				bool prevMipMapDebugMode = ms_mipMapDebugMode;
				ImGui::Checkbox("Mip Map Debug Mode", &ms_mipMapDebugMode);
				if (prevMipMapDebugMode != ms_mipMapDebugMode)
				{
					ResourceTracker::ClearTexList();
					ResourceTracker::ClearMatList();
					TextureLoader::Shutdown();
					UnloadContent();
					LoadContent();
				}
			}	

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Window");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				bool fullScreen = m_pWindow->IsFullScreen();
				ImGui::Checkbox("Fullscreen", &fullScreen);
				m_pWindow->SetFullscreen(fullScreen);

				bool vSync = m_pWindow->IsVSync();
				ImGui::Checkbox("VSync", &vSync);
				m_pWindow->SetVSync(vSync);
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Assets");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				bool prevForceReload = ms_forceReloadBinTex;
				ImGui::Checkbox("Force reload all binTex", &ms_forceReloadBinTex);
				if (!prevForceReload && ms_forceReloadBinTex)
				{
					ResourceTracker::ClearTexList();
					ResourceTracker::ClearMatList();
				}					
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("ImGUI");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				ImGui::Checkbox("Show demo window", &m_showImGUIDemo);
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::TreePop();
			ImGui::Spacing();
		}

		if (ImGui::TreeNode("Scene##2"))
		{
			ImGui::SeparatorText("Visuals");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::ColorEdit4("Background Color", m_backgroundColor);

			ImGui::Checkbox("Debug Lines", &ms_renderDebugLines);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Rendering");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::Checkbox("DSV enabled", &m_dsvEnabled);

			ImGui::Checkbox("Sort By Depth", &ms_sortBatchGos);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Directional Light CBV");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::InputFloat3("Direction", reinterpret_cast<float*>(&m_perFrameCBuffers.DirectionalLight.LightDirection));
			m_perFrameCBuffers.DirectionalLight.LightDirection = Normalize(m_perFrameCBuffers.DirectionalLight.LightDirection);

			ImGui::ColorEdit4("Light Colour", reinterpret_cast<float*>(&m_perFrameCBuffers.DirectionalLight.LightDiffuse));
			ImGui::ColorEdit3("Ambient Colour", reinterpret_cast<float*>(&m_perFrameCBuffers.DirectionalLight.AmbientColor));

			ImGui::InputFloat("Specular Power", &m_perFrameCBuffers.DirectionalLight.SpecularPower);

			ImGui::TreePop();
			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Current Scene"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::SeparatorText("Camera Controls");
			ImGui::Indent(IM_GUI_INDENTATION);

			bool usingFP = m_camera->GetMode() == CameraMode::CAMERA_MODE_FP;
			if (!usingFP)
				ImGui::BeginDisabled(true);

			if (ImGui::Button("Scroll Mode"))
			{
				m_camera->SetMode(CameraMode::CAMERA_MODE_SCROLL);
			}

			if (!usingFP)
				ImGui::EndDisabled();

			if (usingFP)
				ImGui::BeginDisabled(true);

			if (ImGui::Button("WASD Mode"))
			{
				m_camera->SetMode(CameraMode::CAMERA_MODE_FP);
			}

			if (usingFP)
				ImGui::EndDisabled();

			ImGui::InputFloat("Camera Speed", &m_camera->GetSpeedLVal());
			ImGui::InputFloat("Camera Rotation Speed", &m_camera->GetRotSpeedLVal());

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);

			Transform camTrans = m_camera->GetTransform();
			float pPosCam[3] = { camTrans.Position.x, camTrans.Position.y, camTrans.Position.z };
			ImGui::InputFloat3("Position##Camera", pPosCam);
			float pRotCam[3] = { camTrans.Rotation.x, camTrans.Rotation.y, camTrans.Rotation.z };
			ImGui::InputFloat3("Rotation##Camera", pRotCam);
			camTrans.Position = XMFLOAT3(pPosCam[0], pPosCam[1], pPosCam[2]);
			camTrans.Rotation = XMFLOAT3(pRotCam[0], pRotCam[1], pRotCam[2]);

			if (ImGui::Button("Reset to Default"))
			{
				camTrans.Position = XMFLOAT3(0, 0, -10);
				camTrans.Rotation = XMFLOAT3(0, 0, 0);
			}

			m_camera->SetTransform(camTrans);

			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		size_t goCount = 0;
		auto& batchList = ResourceTracker::GetBatches();
		for (auto& it : batchList)
		{
			goCount += it.second->GetOpaques().size();
			goCount += it.second->GetTrans().size();
		}

		string goTag = "GameObjects (" + std::to_string(goCount) + ")";
		if (ImGui::CollapsingHeader(goTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);			
			
			for (auto& it : batchList)
			{
				auto& trans = it.second->GetOpaques();
				for (size_t i = 0; i < trans.size(); i++)
				{
					if (ImGui::CollapsingHeader(trans[i].m_Name.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						Transform t = trans[i].GetTransform();

						string iStr = "##" + std::to_string(i);

						float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
						ImGui::InputFloat3(("Position" + iStr).c_str(), pPos);
						float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
						ImGui::InputFloat3(("Rotation" + iStr).c_str(), pRot);
						float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
						ImGui::InputFloat3(("Scale" + iStr).c_str(), pScale);

						t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
						t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
						t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

						trans[i].SetTransform(t);

						if (ImGui::TreeNode(("Model Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							string vCountStr = "Model Vertex Count: " + std::to_string(trans[i].GetModelVertexCount());
							string iCountStr = "Model Index Count: " + std::to_string(trans[i].GetModelIndexCount());

							XMFLOAT3 pos;
							float radius;
							trans[i].GetBoundingSphere(pos, radius);
							string radiStr = "Model Bounding Radius: " + std::to_string(radius);
							string centroidStr = "Model Centroid: " + ToString(pos);

							ImGui::Text(vCountStr.c_str());
							ImGui::Text(iCountStr.c_str());
							ImGui::Text(radiStr.c_str());
							ImGui::Text(centroidStr.c_str());

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						if (ImGui::TreeNode(("Shader Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							wstring vs, ps, hs, ds;
							trans[i].GetShaderNames(vs, ps, hs, ds);
							vs = L"VS: " + vs;
							ps = L"PS: " + ps;
							hs = L"HS: " + hs;
							ds = L"DS: " + ds;
							ImGui::Text(wstringToString(vs).c_str());
							ImGui::Text(wstringToString(ps).c_str());
							ImGui::Text(wstringToString(hs).c_str());
							ImGui::Text(wstringToString(ds).c_str());

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						if (ImGui::TreeNode(("Texture Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							auto& textures = trans[i].GetMaterial()->GetTextures();
							for (int i = 0; i < textures.size(); i++)
							{
								ImGui::Text((std::to_string(i) + ". " + textures[i]->GetFilePath()).c_str());

								ImGui::Indent(IM_GUI_INDENTATION);
								ImGui::Text(("Mip Levels: " + std::to_string(textures[i]->GetMipLevels())).c_str());
								ImGui::Unindent(IM_GUI_INDENTATION);
							}

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}
				}

				auto& transGos = it.second->GetTrans();
				for (size_t i = 0; i < transGos.size(); i++)
				{
					if (ImGui::CollapsingHeader(transGos[i].m_Name.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						Transform t = transGos[i].GetTransform();

						string iStr = "##" + std::to_string(i);

						float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
						ImGui::InputFloat3(("Position" + iStr).c_str(), pPos);
						float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
						ImGui::InputFloat3(("Rotation" + iStr).c_str(), pRot);
						float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
						ImGui::InputFloat3(("Scale" + iStr).c_str(), pScale);

						t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
						t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
						t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

						transGos[i].SetTransform(t);

						if (ImGui::TreeNode(("Model Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							string vCountStr = "Model Vertex Count: " + std::to_string(transGos[i].GetModelVertexCount());
							string iCountStr = "Model Index Count: " + std::to_string(transGos[i].GetModelIndexCount());

							XMFLOAT3 pos;
							float radius;
							transGos[i].GetBoundingSphere(pos, radius);
							string radiStr = "Model Bounding Radius: " + std::to_string(radius);
							string centroidStr = "Model Centroid: " + ToString(pos);

							ImGui::Text(vCountStr.c_str());
							ImGui::Text(iCountStr.c_str());
							ImGui::Text(radiStr.c_str());
							ImGui::Text(centroidStr.c_str());

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						if (ImGui::TreeNode(("Shader Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							wstring vs, ps, hs, ds;
							transGos[i].GetShaderNames(vs, ps, hs, ds);
							vs = L"VS: " + vs;
							ps = L"PS: " + ps;
							hs = L"HS: " + hs;
							ds = L"DS: " + ds;
							ImGui::Text(wstringToString(vs).c_str());
							ImGui::Text(wstringToString(ps).c_str());
							ImGui::Text(wstringToString(hs).c_str());
							ImGui::Text(wstringToString(ds).c_str());

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						if (ImGui::TreeNode(("Texture Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							auto& textures = transGos[i].GetMaterial()->GetTextures();
							for (int i = 0; i < textures.size(); i++)
							{
								ImGui::Text((std::to_string(i) + ". " + textures[i]->GetFilePath()).c_str());

								ImGui::Indent(IM_GUI_INDENTATION);
								ImGui::Text(("Mip Levels: " + std::to_string(textures[i]->GetMipLevels())).c_str());
								ImGui::Unindent(IM_GUI_INDENTATION);
							}

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}
				}
			}		

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::Spacing();
		}

		string batchTag = "Batches (" + std::to_string(batchList.size()) + ")";
		if (ImGui::CollapsingHeader(batchTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : batchList)
			{
				if (ImGui::CollapsingHeader(it.second->m_Name.c_str()))
				{
					ImGui::Indent(IM_GUI_INDENTATION);

					auto opaques = it.second->GetOpaques();
					string tag = "Opaques (" + std::to_string(opaques.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < opaques.size(); i++)
						{
							ImGui::Text(opaques[i].m_Name.c_str());
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					auto trans = it.second->GetTrans();
					tag = "Transparents (" + std::to_string(trans.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < trans.size(); i++)
						{
							ImGui::Text(trans[i].m_Name.c_str());
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					ImGui::Spacing();
					ImGui::Unindent(IM_GUI_INDENTATION);
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& texList = ResourceTracker::GetTextures();
		string texTag = "Textures (" + std::to_string(texList.size()) + ")";
		if (ImGui::CollapsingHeader(texTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : texList)
			{
				ImGui::Text(it.first.c_str());

				ImGui::Indent(IM_GUI_INDENTATION);
				ImGui::Text(("Mip Levels: " + std::to_string(it.second->GetMipLevels())).c_str());
				ImGui::Text(("Channels: " + std::to_string(it.second->GetChannels())).c_str());
				if (it.second->GetHasAlpha())
					ImGui::Text("Transparent: TRUE");

				ImGui::Unindent(IM_GUI_INDENTATION);
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& modelList = ResourceTracker::GetModels();
		string modelTag = "Models (" + std::to_string(modelList.size()) + ")";
		if (ImGui::CollapsingHeader(modelTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : modelList)
			{
				ImGui::Text(it.first.c_str());
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}
		
		string shaderTag = "Shaders (" + std::to_string(shaderList.size()) + ")";
		if (ImGui::CollapsingHeader(shaderTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : shaderList)
			{
				ImGui::Text(it.first.c_str());

				if (it.second->IsPreCompiled())
				{
					ImGui::Indent(IM_GUI_INDENTATION);

					ImGui::Text("Precompiled: TRUE");

					ImGui::Unindent(IM_GUI_INDENTATION);
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& matList = ResourceTracker::GetMaterials();
		string matTag = "Materials (" + std::to_string(matList.size()) + ")";
		if (ImGui::CollapsingHeader(matTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (size_t i = 0; i < matList.size(); i++)
			{
				UINT srv, cbvFrame, cbvDraw;
				matList[i]->GetIndices(srv, cbvFrame, cbvDraw);

				ImGui::Text(("Material: " + std::to_string(i)).c_str());

				ImGui::Indent(IM_GUI_INDENTATION);
				
				if (cbvFrame != -1)
					ImGui::Text(("CBV Index (PerFrame): " + std::to_string(cbvFrame)).c_str());
				if (cbvDraw != -1)
					ImGui::Text(("CBV Index (PerDraw): " + std::to_string(cbvDraw)).c_str());
				if (srv != -1)
					ImGui::Text(("SRV Index: " + std::to_string(srv)).c_str());

				ImGui::Indent(IM_GUI_INDENTATION);

				auto& matTexList = matList[i]->GetTextures();
				for (size_t t = 0; t < matTexList.size(); t++)
				{
					ImGui::Text(("SRV " + std::to_string(t) + ": " + matTexList[t]->GetFilePath()).c_str());
				}
				
				ImGui::Unindent(IM_GUI_INDENTATION);

				MaterialPropertiesCB matProp;
				if (matList[i]->GetProperties(matProp))
				{
					ImGui::Text("Material Properties:");

					ImGui::Indent(IM_GUI_INDENTATION);

					ImGui::Text(("BaseColor: " + ToString(matProp.BaseColorFactor)).c_str());
					ImGui::Text(("Roughness: " + std::to_string(matProp.Roughness)).c_str());
					ImGui::Text(("Metalness: " + std::to_string(matProp.Metallic)).c_str());
					ImGui::Text(("Alpha Cutoff: " + std::to_string(matProp.AlphaCutoff)).c_str());
					ImGui::Text(("Dispersion: " + std::to_string(matProp.Dispersion)).c_str());
					ImGui::Text(("IOR: " + std::to_string(matProp.IOR)).c_str());

					ImGui::Unindent(IM_GUI_INDENTATION);
				}

				ImGui::Unindent(IM_GUI_INDENTATION);
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		if (!DescriptorManager::ms_DebugHeapEnabled)
			ImGui::BeginDisabled(true);

		if (ImGui::CollapsingHeader("Heap View"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			auto list = DescriptorManager::GetDebugHeapStrings();

			for (size_t i = 0; i < list.size(); i++)
			{
				ImGui::Text((std::to_string(i) + ". " + list[i]).c_str());
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		if (!DescriptorManager::ms_DebugHeapEnabled)
			ImGui::EndDisabled();

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	if (m_showImGUIDemo)
		ImGui::ShowDemoWindow(); // Show demo window! :)
}