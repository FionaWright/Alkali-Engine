#include "pch.h"
#include "SceneBistro.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "CBuffers.h"

SceneBistro::SceneBistro(const std::wstring& name, shared_ptr<Window> pWindow)
	: Scene(name, pWindow, true)
	, m_FoV(45.0f)
{
}

bool SceneBistro::LoadContent()
{
	Scene::LoadContent();

	auto commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	int numDescriptors = 2;
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numDescriptors, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // 2 Textures

	const int paramCount = 2;
	CD3DX12_ROOT_PARAMETER1 rootParameters[paramCount];
	rootParameters[0].InitAsConstants(sizeof(MatricesCB) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsDescriptorTable(_countof(ranges), &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_STATIC_SAMPLER_DESC sampler[1]; 
	sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].MipLODBias = 0;
	sampler[0].MaxAnisotropy = 0;
	sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler[0].MinLOD = 0.0f;
	sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
	sampler[0].ShaderRegister = 0;
	sampler[0].RegisterSpace = 0;
	sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	auto rootSig = ResourceManager::CreateRootSignature(rootParameters, paramCount, sampler, 1);	

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_shaderPBR = std::make_shared<Shader>();
	m_shaderPBR->Init(L"PBR.vs", L"PBR.ps", inputLayout, _countof(inputLayout), rootSig.Get(), m_d3dClass->GetDevice());

	m_batch = std::make_shared<Batch>(rootSig);
	ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderPBR);
	m_batch->AddHeldGameObjectsToList(m_gameObjectList);

	auto fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

	m_camera->SetPosition(0, 0, -10);
	m_camera->SetRotation(0, 0, 0);

	return true;
}

void SceneBistro::UnloadContent()
{
	Scene::UnloadContent();

	m_FoV = 45;
	m_batch.reset();
	m_shaderPBR.reset();
}

void SceneBistro::OnUpdate(TimeEventArgs& e)
{
	Scene::OnUpdate(e);

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
}

void SceneBistro::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);

	auto commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetAvailableCommandList();

	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtvCPUDesc = m_pWindow->GetCurrentRenderTargetView();
	auto dsvCPUDesc = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ClearBackBuffer(commandList.Get());

	m_batch->Render(commandList.Get(), m_viewport, m_scissorRect, rtvCPUDesc, dsvCPUDesc, m_viewProjMatrix, m_frustum);

	Scene::RenderDebugLines(commandList.Get(), rtvCPUDesc, dsvCPUDesc);

	ImGUIManager::Render(commandList.Get());

	Present(commandList.Get(), commandQueue);
}
