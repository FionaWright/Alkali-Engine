#include "pch.h"
#include "CubeScene.h"
#include "ImGUIManager.h"

CubeScene::CubeScene(const std::wstring& name, shared_ptr<Window> pWindow)
	: Scene(name, pWindow, true)
	, m_FoV(45.0f)
{
}

bool CubeScene::LoadContent()
{
	auto commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList = commandQueue->GetAvailableCommandList();

	m_modelCube = std::make_shared<Model>();
	m_modelCube->Init(commandList.Get(), L"Cube.model");

	// A single 32-bit constant root parameter that is used by the vertex shader.
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	auto rootSig = ResourceManager::CreateRootSignature(rootParameters, 1, nullptr, 0);

	m_batch = std::make_shared<Batch>(rootSig);

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_shaderCube = std::make_shared<Shader>();
	m_shaderCube->Init(L"Test.vs", L"Test.ps", inputLayout, _countof(inputLayout), rootSig.Get(), m_d3dClass->GetDevice());
	//m_shaderCube->InitPreCompiled(L"Test_VS.cso", L"Test_PS.cso", inputLayout, _countof(inputLayout), rootSig);

	m_goCube = std::make_shared<GameObject>("Cube", m_modelCube, m_shaderCube);
	m_gameObjectList.push_back(m_goCube.get());

	m_batch->AddGameObject(m_goCube);

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	m_camera->SetPosition(0, 0, -10);
	m_camera->SetRotation(0, 0, 0);

	return true;
}

void CubeScene::UnloadContent()
{
	Scene::UnloadContent();

	m_FoV = 45;
	m_modelCube.reset();
	m_batch.reset();
	m_shaderCube.reset();
	m_goCube.reset();
}

void CubeScene::OnUpdate(TimeEventArgs& e)
{
	Scene::OnUpdate(e);

	XMFLOAT2 mousePos = InputManager::GetMousePos();

	float angle = static_cast<float>(e.TotalTime * 5.0);
	m_goCube->SetRotation(0, angle, 0);

	m_viewMatrix = m_camera->GetViewMatrix();
	m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), ASPECT_RATIO, 0.1f, 100.0f);

	if (InputManager::IsKeyDown(KeyCode::Escape))
	{
		PostQuitMessage(0);
	}

	bool altEnter = InputManager::IsKeyDown(KeyCode::Enter) && InputManager::IsAltHeld();
	if (altEnter || InputManager::IsKeyDown(KeyCode::F11))
	{
		m_pWindow->ToggleFullscreen();
	}

	if (InputManager::IsKeyDown(KeyCode::V))
	{
		m_pWindow->ToggleVSync();
	}

	m_FoV -= InputManager::GetMouseWheelDelta();
	m_FoV = std::clamp(m_FoV, 12.0f, 90.0f);
}

void CubeScene::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);

	auto commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetAvailableCommandList();

	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtvCPUDesc = m_pWindow->GetCurrentRenderTargetView();
	auto dsvCPUDesc = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ClearBackBuffer(commandList.Get());

	XMMATRIX viewProj = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);

	m_batch->Render(commandList.Get(), m_viewport, m_scissorRect, rtvCPUDesc, dsvCPUDesc, viewProj);

	ImGUIManager::Render(commandList.Get());

	Present(commandList.Get(), commandQueue);
}
