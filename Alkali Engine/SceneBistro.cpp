#include "pch.h"
#include "SceneBistro.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "CBuffers.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"

SceneBistro::SceneBistro(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
{
}

bool SceneBistro::LoadContent()
{
	Scene::LoadContent();

	CommandQueue* commandQueueCopy = nullptr;
	commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	if (!commandQueueCopy)
		throw std::exception("Command Queue Error");

	auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

	shared_ptr<Model> modelInvertedCube = CreateModel("Cube (Inverted).model", commandListCopy.Get());

	auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
	commandQueueCopy->WaitForFenceValue(fenceValue);

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");

	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	vector<string> skyboxPaths = {
			"Skyboxes/Iceland/negx.tga",
			"Skyboxes/Iceland/posx.tga",
			"Skyboxes/Iceland/posy.tga",
			"Skyboxes/Iceland/negy.tga",
			"Skyboxes/Iceland/negz.tga",
			"Skyboxes/Iceland/posz.tga"
	};

	shared_ptr<Texture> skyboxTex = CreateCubemap(skyboxPaths, commandListDirect.Get());
	shared_ptr<Texture> irradianceTex = CreateIrradianceMap(skyboxTex.get(), commandListDirect.Get());

	RootParamInfo rootParamInfo;
	rootParamInfo.NumCBV_PerFrame = 2;
	rootParamInfo.NumCBV_PerDraw = 2;
	rootParamInfo.NumSRV = 5;
	rootParamInfo.ParamIndexCBV_PerDraw = 0;
	rootParamInfo.ParamIndexCBV_PerFrame = 1;
	rootParamInfo.ParamIndexSRV = 2;

	shared_ptr<RootSig> rootSigPBR = std::make_shared<RootSig>();
	rootSigPBR->InitDefaultSampler("PBR Root Sig", rootParamInfo);

	RootParamInfo rootParamInfoSkybox;
	rootParamInfoSkybox.NumCBV_PerDraw = 1;
	rootParamInfoSkybox.NumSRV = 1;
	rootParamInfoSkybox.ParamIndexCBV_PerDraw = 0;
	rootParamInfoSkybox.ParamIndexSRV = 1;

	shared_ptr<RootSig> rootSigSkybox = std::make_shared<RootSig>();
	rootSigSkybox->InitDefaultSampler("Skybox Root Sig", rootParamInfoSkybox);

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
	vector<shared_ptr<Texture>> textures = { skyboxTex };

	shared_ptr matSkybox = std::make_shared<Material>();
	matSkybox->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
	matSkybox->AddSRVs(m_d3dClass, textures);
	ResourceTracker::AddMaterial(matSkybox);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutPBR =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutSkybox =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	shared_ptr<Shader> shaderPBR = CreateShader(L"PBR.vs", L"PBR.ps", inputLayoutPBR, rootSigPBR.get());
	shared_ptr<Shader> shaderPBRCullOff = CreateShader(L"PBR.vs", L"PBR.ps", inputLayoutPBR, rootSigPBR.get(), false, true);
	shared_ptr<Shader> shaderSkybox = CreateShader(L"Skybox_VS.cso", L"Skybox_PS.cso", inputLayoutSkybox, rootSigSkybox.get(), true, false, false, true);

	shared_ptr<Batch> batchPBR = CreateBatch(rootSigPBR);
	shared_ptr<Batch> batchSkybox = CreateBatch(rootSigSkybox);
	
	m_goSkybox = batchSkybox->CreateGameObject("Skybox", modelInvertedCube, shaderSkybox, matSkybox);
	m_goSkybox->SetScale(20);

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderPBR);
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", rootParamInfo, batchPBR.get(), skyboxTex, irradianceTex, shaderPBR, shaderPBRCullOff);

	fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

	m_camera->SetPosition(0, 3, -5);
	m_camera->SetRotation(0, 0, 0);

	return true;
}

void SceneBistro::UnloadContent()
{
	Scene::UnloadContent();

	m_FoV = 45;
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

	m_goSkybox->SetPosition(m_camera->GetWorldPosition());
}

void SceneBistro::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);
}
