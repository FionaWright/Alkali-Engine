#include "pch.h"
#include "SceneBedroom.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "ResourceTracker.h"
#include "Utils.h"
#include "TextureLoader.h"
#include "RootSig.h"
#include "AssetFactory.h"

SceneBedroom::SceneBedroom(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
{
}

bool SceneBedroom::LoadContent()
{
	Scene::LoadContent();

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

	shared_ptr<Texture> skyboxTex = AssetFactory::CreateCubemap(skyboxPaths, commandListDirect.Get());
	shared_ptr<Texture> irradianceTex = AssetFactory::CreateIrradianceMap(skyboxTex.get(), commandListDirect.Get());
	shared_ptr<Texture> blueNoiseTex = AssetFactory::CreateTexture("BlueNoise.png", commandListDirect.Get());
	shared_ptr<Texture> brdfIntTex = AssetFactory::CreateTexture("BRDF Integration Map.png", commandListDirect.Get());

	RootParamInfo rootParamInfoPBR;
	rootParamInfoPBR.NumCBV_PerFrame = 4;
	rootParamInfoPBR.NumCBV_PerDraw = 2;
	rootParamInfoPBR.NumSRV = 7;
	rootParamInfoPBR.NumSRV_Dynamic = 1;
	rootParamInfoPBR.ParamIndexCBV_PerDraw = 0;
	rootParamInfoPBR.ParamIndexCBV_PerFrame = 1;
	rootParamInfoPBR.ParamIndexSRV = 2;
	rootParamInfoPBR.ParamIndexSRV_Dynamic = 3;

	auto rootSigPBR = std::make_shared<RootSig>();
	rootSigPBR->InitDefaultSampler("PBR Root Sig", rootParamInfoPBR);

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

	ShaderArgs argsPBR = { L"PBR.vs", L"PBR.ps", inputLayoutPBR, rootSigPBR->GetRootSigResource() };
	shared_ptr<Shader> shaderPBR = AssetFactory::CreateShader(argsPBR);

	argsPBR.CullNone = true;
	shared_ptr<Shader> shaderPBRCullOff = AssetFactory::CreateShader(argsPBR);

	shared_ptr<Batch> batchPBR = AssetFactory::CreateBatch(rootSigPBR);

	Transform t = { XMFLOAT3_ZERO, XMFLOAT3_ZERO, Mult(XMFLOAT3_ONE, 40) };
	//vector<string> whiteList = { "Pawn_Body_W1", "Pawn_Top_W1"};
	vector<string> whiteList = { };
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Chess.gltf", rootParamInfoPBR, batchPBR.get(), skyboxTex, irradianceTex, shaderPBR, shaderPBRCullOff, &whiteList, t);

	RootParamInfo rootParamInfoSkybox;
	rootParamInfoSkybox.NumCBV_PerDraw = 1;
	rootParamInfoSkybox.NumSRV = 1;
	rootParamInfoSkybox.ParamIndexCBV_PerDraw = 0;
	rootParamInfoSkybox.ParamIndexSRV = 1;

	auto rootSigSkybox = std::make_shared<RootSig>();
	rootSigSkybox->InitDefaultSampler("Skybox Root Sig", rootParamInfoSkybox);

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
	vector<shared_ptr<Texture>> textures = { skyboxTex };

	shared_ptr matSkybox = AssetFactory::CreateMaterial();
	matSkybox->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
	matSkybox->AddSRVs(m_d3dClass, textures);

	ShaderArgs argsSkybox = { L"Skybox_VS.cso", L"Skybox_PS.cso", inputLayoutSkybox, rootSigSkybox->GetRootSigResource() };
	argsSkybox.disableDSVWrite = true;
	shared_ptr<Shader> shaderSkybox = AssetFactory::CreateShader(argsSkybox, true);

	shared_ptr<Model> modelInvertedCube;
	{
		CommandQueue* commandQueueCopy = nullptr;
		commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		modelInvertedCube = AssetFactory::CreateModel("Cube (Inverted).model", commandListCopy.Get());

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	shared_ptr<Batch> batchSkybox = AssetFactory::CreateBatch(rootSigSkybox);
	m_goSkybox = batchSkybox->CreateGameObject("Skybox", modelInvertedCube, shaderSkybox, matSkybox);
	m_goSkybox->SetScale(20);
	m_goSkybox->SetOccluderState(false);

	m_camera->SetPosition(16, 6, -5);
	m_camera->SetRotation(0, -90, 0);

	auto fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

	m_perFrameCBuffers.DirectionalLight.LightDirection = XMFLOAT3(0, -1, 0);

	return true;
}

void SceneBedroom::UnloadContent()
{
	Scene::UnloadContent();
}

void SceneBedroom::OnUpdate(TimeEventArgs& e)
{
	Scene::OnUpdate(e);

	XMFLOAT2 mousePos = InputManager::GetMousePos();

	float angle = static_cast<float>(e.TotalTime * 0.2f);
	//m_goCube->RotateBy(0, angle, 0);
	//m_perFrameCBuffers.DirectionalLight.LightDirection = Normalize(XMFLOAT3(cos(angle), -0.5f, sin(angle)));

	if (InputManager::IsKeyDown(KeyCode::Escape))
	{
		PostQuitMessage(0);
	}

	bool altEnter = InputManager::IsKeyDown(KeyCode::Enter) && InputManager::IsAltHeld();
	if (altEnter || InputManager::IsKeyDown(KeyCode::F11))
	{
		SettingsManager::ms_Dynamic.FullscreenEnabled = !SettingsManager::ms_Dynamic.FullscreenEnabled;
		m_pWindow->SetFullscreen(SettingsManager::ms_Dynamic.FullscreenEnabled);
	}

	if (InputManager::IsKeyDown(KeyCode::V))
	{
		SettingsManager::ms_Dynamic.VSyncEnabled = !SettingsManager::ms_Dynamic.VSyncEnabled;
	}

	m_goSkybox->SetPosition(m_camera->GetWorldPosition());
}

void SceneBedroom::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);
}