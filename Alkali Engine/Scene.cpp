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

shared_ptr<Model> Scene::ms_sphereModel;
shared_ptr<Material> Scene::ms_perFramePBRMat;

Scene::Scene(const std::wstring& name, Window* pWindow, bool createDSV)
	: m_Name(name)
	, m_pWindow(pWindow)
	, m_dsvEnabled(createDSV)
	, m_camera(std::make_unique<Camera>(CameraMode::CAMERA_MODE_SCROLL))
	, m_viewMatrix(XMMatrixIdentity())
	, m_projectionMatrix(XMMatrixIdentity())
	, m_viewProjMatrix(XMMatrixIdentity())
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

	m_perFrameCBuffers.DirectionalLight.AmbientColor = XMFLOAT3(0.15f, 0.15f, 0.15f);
	m_perFrameCBuffers.DirectionalLight.LightDiffuse = XMFLOAT3(1, 1, 1);
	m_perFrameCBuffers.DirectionalLight.LightDirection = Normalize(XMFLOAT3(0.5f, -0.5f, 0.5f));
	m_perFrameCBuffers.DirectionalLight.SpecularPower = 32.0f;

	DescriptorManager::Init(m_d3dClass, SettingsManager::ms_DX12.DescriptorHeapSize);

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

		ms_sphereModel = AssetFactory::CreateModel("Sphere.model", commandListCopy.Get());
		modelPlane = AssetFactory::CreateModel("Plane.model", commandListCopy.Get());

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

	m_rpiLine.NumCBV_PerDraw = 1;
	m_rpiLine.ParamIndexCBV_PerDraw = 0;


	m_rootSigLine = std::make_shared<RootSig>();
	m_rootSigLine->InitDefaultSampler("Line Root Sig", m_rpiLine);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	m_shaderLine = AssetFactory::CreateShader(L"Line_VS.cso", L"Line_PS.cso", inputLayout, m_rootSigLine.get(), true, false, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

	{
		vector<UINT> cbvSizesDraw = { sizeof(MatricesLineCB) };

		m_matLine = std::make_shared<Material>();
		m_matLine->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
		ResourceTracker::AddMaterial(m_matLine);
	}

	{
		vector<DebugLine*> frustumDebugLines;
		for (int i = 0; i < 12; i++)
			frustumDebugLines.push_back(AddDebugLine(XMFLOAT3_ZERO, XMFLOAT3_ZERO, XMFLOAT3_ONE));

		m_frustum.SetDebugLines(frustumDebugLines);

		XMFLOAT3& lDir = m_perFrameCBuffers.DirectionalLight.LightDirection;
		m_debugLineLightDir = AddDebugLine(Mult(lDir, 999), Mult(lDir, -999), XMFLOAT3(1, 1, 0));
	}

	m_rpiDepth.NumSRV = 1;
	m_rpiDepth.ParamIndexSRV = 0;

	m_rootSigDepth = std::make_shared<RootSig>();
	m_rootSigDepth->InitDefaultSampler("Depth Buffer Root Sig", m_rpiDepth);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDepth =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	m_shaderDepth = AssetFactory::CreateShader(L"Depth_VS.cso", L"Depth_PS.cso", inputLayout, m_rootSigDepth.get(), true, true, true);

	m_depthBufferMat = std::make_shared<Material>();
	m_depthBufferMat->AddDynamicSRVs(1);

	m_goDepthTex = std::make_unique<GameObject>("Depth Tex", modelPlane, m_shaderDepth, m_depthBufferMat, true);
	m_goDepthTex->SetRotation(90, 0, 0);

	auto fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

    return true;
}

void Scene::UnloadContent()
{
	m_BackgroundColor = XMFLOAT4(0.4f, 0.6f, 0.9f, 1.0f);
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

	float aspectRatio = SettingsManager::ms_Window.ScreenWidth / SettingsManager::ms_Window.ScreenHeight;
	m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(SettingsManager::ms_Window.FieldOfView), aspectRatio, SettingsManager::ms_DX12.NearPlane, SettingsManager::ms_DX12.FarPlane);
	m_viewProjMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);

	if (!SettingsManager::ms_Dynamic.FreezeFrustum)
		m_frustum.UpdateValues(m_viewProjMatrix);
	
	bool showLines = SettingsManager::ms_Dynamic.AlwaysShowFrustumDebugLines || SettingsManager::ms_Dynamic.FreezeFrustum;
	if (showLines)
		m_frustum.CalculateDebugLinePoints(m_d3dClass);

	m_frustum.SetDebugLinesEnabled(showLines);

	XMFLOAT3& lDir = m_perFrameCBuffers.DirectionalLight.LightDirection;
	m_debugLineLightDir->SetPositions(m_d3dClass, Mult(lDir, 999), Mult(lDir, -999));

	if (SettingsManager::ms_Dynamic.BatchSortingEnabled)
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

	if (SettingsManager::ms_Dynamic.DebugLinesEnabled)
		RenderDebugLines(commandList.Get(), rtvCPUDesc, dsvCPUDesc);

	if (SettingsManager::ms_Dynamic.VisualiseDSVEnabled && m_goDepthTex)
	{
		commandList->OMSetRenderTargets(numRenderTargets, &rtvCPUDesc, FALSE, nullptr); // Disable DSV

		commandList->SetGraphicsRootSignature(m_rootSigDepth->GetRootSigResource());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Convert DSV to SRV and assign as a texture to be read in the shader
		ResourceManager::TransitionResource(commandList.Get(), m_depthBufferResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_depthBufferMat->SetDynamicSRV(m_d3dClass, 0, DXGI_FORMAT_R32_FLOAT, m_depthBufferResource.Get());

		m_depthBufferMat->AssignMaterial(commandList.Get(), m_rpiDepth);

		m_goDepthTex->Render(commandList.Get(), m_rpiDepth);
		
		commandList->OMSetRenderTargets(numRenderTargets, &rtvCPUDesc, FALSE, &dsvCPUDesc);
	}

	ImGUIManager::Render(commandList.Get());

	UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
	Present(commandList.Get(), commandQueue); 
	
	// Forced to wait for execution of current back buffer command list so we can transition the depth buffer back
	if (SettingsManager::ms_Dynamic.VisualiseDSVEnabled && m_goDepthTex)
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

bool Scene::IsSphereModeOn(Model** model)
{
	if (!SettingsManager::ms_Dynamic.BoundingSphereMode)
		return false;

	*model = ms_sphereModel.get();
	return true;
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

PerFrameCBuffers& Scene::GetPerFrameCBuffers()
{
	return m_perFrameCBuffers;
}

Camera* Scene::GetCamera()
{
	return m_camera.get();
}

void Scene::ClearBackBuffer(ID3D12GraphicsCommandList2* commandList)
{
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->ClearRenderTargetView(rtv, reinterpret_cast<FLOAT*>(&m_BackgroundColor), 0, nullptr);
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
	optimizedClearValue.Format = SettingsManager::ms_DX12.DSVFormat;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto dsvResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(optimizedClearValue.Format, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &dsvResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&m_depthBufferResource));
	ThrowIfFailed(hr);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = optimizedClearValue.Format;
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

void Scene::RenderDebugLines(ID3D12GraphicsCommandList2* commandListDirect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	for (int i = 0; i < m_debugLineList.size(); i++)
	{
		m_debugLineList.at(i)->Render(commandListDirect, m_rootSigLine->GetRootSigResource(), m_viewport, m_scissorRect, rtv, dsv, m_viewProjMatrix);
	}
}