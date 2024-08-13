#include "pch.h"
#include "SceneChess.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "ResourceTracker.h"
#include "Utils.h"
#include "TextureLoader.h"
#include "RootSig.h"
#include "AssetFactory.h"

SceneChess::SceneChess(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
{
}

bool SceneChess::LoadContent()
{
	Scene::LoadContent();

	CommandQueue* cmdQueueDirect = nullptr;
	cmdQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!cmdQueueDirect)
		throw std::exception("Command Queue Error");
	auto cmdListDirect = cmdQueueDirect->GetAvailableCommandList();	

	vector<string> skyboxPaths = {
		"Skyboxes/Iceland/negx.tga",
		"Skyboxes/Iceland/posx.tga",
		"Skyboxes/Iceland/posy.tga",
		"Skyboxes/Iceland/negy.tga",
		"Skyboxes/Iceland/negz.tga",
		"Skyboxes/Iceland/posz.tga"
	};

	shared_ptr<Texture> skyboxTex = AssetFactory::CreateCubemap(skyboxPaths, cmdListDirect.Get());
	shared_ptr<Texture> irradianceTex = AssetFactory::CreateIrradianceMap(skyboxTex.get(), cmdListDirect.Get());
	shared_ptr<Texture> blueNoiseTex = AssetFactory::CreateTexture("BlueNoise.png", cmdListDirect.Get());
	shared_ptr<Texture> brdfIntTex = AssetFactory::CreateTexture("BRDF Integration Map.png", cmdListDirect.Get());

	m_perFrameCBuffers.EnvMap.EnvMapMipLevels = skyboxTex->GetMipLevels();

	RootParamInfo rootParamInfoPBR;
	rootParamInfoPBR.NumCBV_PerFrame = 5;
	rootParamInfoPBR.NumCBV_PerDraw = 2;
	rootParamInfoPBR.NumSRV = 7;
	rootParamInfoPBR.NumSRV_Dynamic = 1;
	rootParamInfoPBR.ParamIndexCBV_PerDraw = 0;
	rootParamInfoPBR.ParamIndexCBV_PerFrame = 1;
	rootParamInfoPBR.ParamIndexSRV = 2;
	rootParamInfoPBR.ParamIndexSRV_Dynamic = 3;

	D3D12_STATIC_SAMPLER_DESC samplerDesc[2];
	samplerDesc[0] = SettingsManager::ms_DX12.DefaultSamplerDesc;
	samplerDesc[1] = SettingsManager::ms_DX12.DefaultSamplerDesc;
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1;

	auto rootSigPBR = std::make_shared<RootSig>();
	rootSigPBR->Init("PBR Root Sig", rootParamInfoPBR, samplerDesc, _countof(samplerDesc));

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
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, cmdListDirect.Get(), "Chess.gltf", rootParamInfoPBR, batchPBR.get(), skyboxTex, irradianceTex, shaderPBR, shaderPBRCullOff, &whiteList, t);

	RootParamInfo rootParamInfoSkybox;
	rootParamInfoSkybox.NumCBV_PerDraw = 1;
	rootParamInfoSkybox.NumSRV = 1;
	rootParamInfoSkybox.ParamIndexCBV_PerDraw = 0;
	rootParamInfoSkybox.ParamIndexSRV = 1;

	auto rootSigSkybox = std::make_shared<RootSig>();
	rootSigSkybox->Init("Skybox Root Sig", rootParamInfoSkybox, &SettingsManager::ms_DX12.DefaultSamplerDesc, 1);

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
	vector<shared_ptr<Texture>> textures = { skyboxTex };

	shared_ptr matSkybox = AssetFactory::CreateMaterial();
	matSkybox->AddCBVs(m_d3dClass, cmdListDirect.Get(), cbvSizesDraw, false);
	matSkybox->AddSRVs(m_d3dClass, textures);

	ShaderArgs argsSkybox = { L"Skybox_VS.cso", L"Skybox_PS.cso", inputLayoutSkybox, rootSigSkybox->GetRootSigResource() };
	argsSkybox.disableDSVWrite = true;
	shared_ptr<Shader> shaderSkybox = AssetFactory::CreateShader(argsSkybox, true);

	shared_ptr<Model> modelInvertedCube;
	{
		CommandQueue* cmdQueueCopy = nullptr;
		cmdQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!cmdQueueCopy)
			throw std::exception("Command Queue Error");

		auto cmdListCopy = cmdQueueCopy->GetAvailableCommandList();

		modelInvertedCube = AssetFactory::CreateModel("Cube (Inverted).model", cmdListCopy.Get());

		auto fenceValue = cmdQueueCopy->ExecuteCommandList(cmdListCopy);
		cmdQueueCopy->WaitForFenceValue(fenceValue);
	}

	shared_ptr<Batch> batchSkybox = AssetFactory::CreateBatch(rootSigSkybox);
	m_goSkybox = batchSkybox->CreateGameObject("Skybox", modelInvertedCube, shaderSkybox, matSkybox);
	m_goSkybox->SetScale(20);
	m_goSkybox->SetOccluderState(false);

	m_camera->SetPosition(16, 6, -5);
	m_camera->SetRotation(0, -90, 0);

	auto fenceValue = cmdQueueDirect->ExecuteCommandList(cmdListDirect);
	cmdQueueDirect->WaitForFenceValue(fenceValue);

	m_perFrameCBuffers.DirectionalLight.LightDirection = XMFLOAT3(1, -1, 0);

	return true;
}

void SceneChess::UnloadContent()
{
	Scene::UnloadContent();
}

void SceneChess::OnUpdate(TimeEventArgs& e)
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

void SceneChess::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);
}