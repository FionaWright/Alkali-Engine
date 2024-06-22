#include "pch.h"
#include "SceneBistro.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "CBuffers.h"
#include "ResourceTracker.h"

SceneBistro::SceneBistro(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
	, m_FoV(45.0f)
{
}

bool SceneBistro::LoadContent()
{
	Scene::LoadContent();

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");

	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	int numDescriptors = 2;
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numDescriptors, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // 2 Textures

	const int paramCount = 2;
	CD3DX12_ROOT_PARAMETER1 rootParameters[paramCount];
	rootParameters[0].InitAsConstants(sizeof(MatricesCB) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsDescriptorTable(_countof(ranges), &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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

	auto rootSig = ResourceManager::CreateRootSignature(rootParameters, paramCount, sampler, 1);	

	if (!ResourceTracker::TryGetShader("PBR.vs - PBR.ps", m_shaderPBR))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		m_shaderPBR->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSig.Get(), m_d3dClass->GetDevice());
		//m_shaderCube->InitPreCompiled(L"Test_VS.cso", L"Test_PS.cso", inputLayout, _countof(inputLayout), rootSig);
	}

	if (!ResourceTracker::TryGetShader("PBR.vs - PBR.ps --CullOff", m_shaderPBR_CullOff))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		m_shaderPBR_CullOff->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSig.Get(), m_d3dClass->GetDevice(), true);
	}

	if (!ResourceTracker::TryGetBatch("PBR Basic", m_batch))
	{
		m_batch->Init("PBR Basic", rootSig);
	}

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderPBR);
	ModelLoader::LoadModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", m_batch.get(), m_shaderPBR, m_shaderPBR_CullOff);

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
}
