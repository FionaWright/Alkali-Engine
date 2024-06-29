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

	RootParamInfo rootParamInfo;
	rootParamInfo.NumCBV_PerFrame = 2;
	rootParamInfo.NumCBV_PerDraw = 2;
	rootParamInfo.NumSRV = 3;
	rootParamInfo.ParamIndexCBV_PerDraw = 0;
	rootParamInfo.ParamIndexCBV_PerFrame = 1;
	rootParamInfo.ParamIndexSRV = 2;

	ComPtr<ID3D12RootSignature> rootSigPBR;
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		int shaderRegisterFrameStart = rootParamInfo.NumCBV_PerDraw;
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rootParamInfo.NumCBV_PerDraw, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rootParamInfo.NumCBV_PerFrame, shaderRegisterFrameStart, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rootParamInfo.NumSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[rootParamInfo.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[rootParamInfo.ParamIndexCBV_PerFrame].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[rootParamInfo.ParamIndexSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

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

		rootSigPBR = ResourceManager::CreateRootSignature(rootParameters, _countof(rootParameters), sampler, _countof(sampler));
		rootSigPBR->SetName(L"Bistro Root Sig");
	}

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

		m_shaderPBR->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSigPBR.Get(), m_d3dClass->GetDevice());
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

		m_shaderPBR_CullOff->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSigPBR.Get(), m_d3dClass->GetDevice(), true);
	}

	if (!ResourceTracker::TryGetBatch("PBR Basic", m_batch))
	{
		m_batch->Init("PBR Basic", rootSigPBR);
	}

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderPBR);
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", rootParamInfo, m_batch.get(), m_shaderPBR, m_shaderPBR_CullOff);

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
