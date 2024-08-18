#include "pch.h"
#include "SceneBistro.h"
#include "ImGUIManager.h"
#include "ModelLoaderObj.h"
#include "ModelLoaderGLTF.h"
#include "CBuffers.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"
#include "AssetFactory.h"

SceneBistro::SceneBistro(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
{
}

bool SceneBistro::LoadContent()
{
	Scene::LoadContent();

	CommandQueue* cmdQueueCopy = nullptr;
	cmdQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	if (!cmdQueueCopy)
		throw std::exception("Command Queue Error");

	auto cmdListCopy = cmdQueueCopy->GetAvailableCommandList();

	shared_ptr<Model> modelInvertedCube = AssetFactory::CreateModel("Cube (Inverted).model", cmdListCopy.Get());

	auto fenceValue = cmdQueueCopy->ExecuteCommandList(cmdListCopy);
	cmdQueueCopy->WaitForFenceValue(fenceValue);

	CommandQueue* cmdQueueDirect = nullptr;
	cmdQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!cmdQueueDirect)
		throw std::exception("Command Queue Error");

	auto cmdListDirect = cmdQueueDirect->GetAvailableCommandList();

	/*vector<string> skyboxPaths = {
			"Skyboxes/Iceland/negx.tga",
			"Skyboxes/Iceland/posx.tga",
			"Skyboxes/Iceland/posy.tga",
			"Skyboxes/Iceland/negy.tga",
			"Skyboxes/Iceland/negz.tga",
			"Skyboxes/Iceland/posz.tga"
	};*/

	//shared_ptr<Texture> skyboxTex = AssetFactory::CreateCubemap(skyboxPaths, cmdListDirect.Get());
	shared_ptr<Texture> skyboxTex = AssetFactory::CreateCubemapHDR("Skyboxes/Bistro_Bridge.hdr", cmdListDirect.Get());
	shared_ptr<Texture> irradianceTex = AssetFactory::CreateIrradianceMap(skyboxTex.get(), cmdListDirect.Get());	

	m_perFrameCBuffers.EnvMap.EnvMapMipLevels = skyboxTex->GetMipLevels();

	RootParamInfo rootParamInfo;
	rootParamInfo.NumCBV_PerFrame = 5;
	rootParamInfo.NumCBV_PerDraw = 3;
	rootParamInfo.NumSRV = 7;
	rootParamInfo.NumSRV_Dynamic = 1;
	rootParamInfo.ParamIndexCBV_PerDraw = 0;
	rootParamInfo.ParamIndexCBV_PerFrame = 1;
	rootParamInfo.ParamIndexSRV = 2;
	rootParamInfo.ParamIndexSRV_Dynamic = 3;

	D3D12_STATIC_SAMPLER_DESC samplerDesc[2];
	samplerDesc[0] = SettingsManager::ms_DX12.DefaultSamplerDesc;
	samplerDesc[1] = SettingsManager::ms_DX12.DefaultSamplerDesc;
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1;

	shared_ptr<RootSig> rootSigPBR = std::make_shared<RootSig>();
	rootSigPBR->Init("PBR Root Sig", rootParamInfo, samplerDesc, _countof(samplerDesc));

	RootParamInfo rootParamInfoSkybox;
	rootParamInfoSkybox.NumCBV_PerDraw = 1;
	rootParamInfoSkybox.NumSRV = 1;
	rootParamInfoSkybox.ParamIndexCBV_PerDraw = 0;
	rootParamInfoSkybox.ParamIndexSRV = 1;

	shared_ptr<RootSig> rootSigSkybox = std::make_shared<RootSig>();
	rootSigSkybox->Init("Skybox Root Sig", rootParamInfoSkybox, &SettingsManager::ms_DX12.DefaultSamplerDesc, 1);

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
	vector<shared_ptr<Texture>> textures = { skyboxTex };

	shared_ptr matSkybox = AssetFactory::CreateMaterial();
	matSkybox->AddCBVs(m_d3dClass, cmdListDirect.Get(), cbvSizesDraw, false);
	matSkybox->AddSRVs(m_d3dClass, textures);

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

	ShaderArgs argsSkybox = { L"Skybox_VS.cso", L"Skybox_PS.cso", inputLayoutSkybox, rootSigSkybox->GetRootSigResource() };
	argsSkybox.DisableDSVWriting = true;
	shared_ptr<Shader> shaderSkybox = AssetFactory::CreateShader(argsSkybox, true);

	shared_ptr<Batch> batchPBR = AssetFactory::CreateBatch(rootSigPBR);
	shared_ptr<Batch> batchSkybox = AssetFactory::CreateBatch(rootSigSkybox);
	
	m_goSkybox = batchSkybox->CreateGameObject("Skybox", modelInvertedCube, shaderSkybox, matSkybox);
	m_goSkybox->SetScale(20);
	m_goSkybox->SetOccluderState(false);

	GLTFLoadArgs gltfArgs;
	gltfArgs.Batches = { batchPBR };
	gltfArgs.Shaders = { shaderPBR, shaderPBRCullOff };
	gltfArgs.SkyboxTex = skyboxTex;
	gltfArgs.IrradianceMap = irradianceTex;

	ModelLoaderGLTF::LoadSplitModel(m_d3dClass, cmdListDirect.Get(), "Bistro.gltf", gltfArgs);
	//ModelLoader::LoadSplitModelGLTF(m_d3dClass, cmdListDirect.Get(), "Bistro.glb", rootParamInfo, batchPBR.get(), skyboxTex, irradianceTex, shaderPBR, shaderPBRCullOff);

	fenceValue = cmdQueueDirect->ExecuteCommandList(cmdListDirect);
	cmdQueueDirect->WaitForFenceValue(fenceValue);

	m_camera->SetPosition(0, 3, -5);
	m_camera->SetRotation(0, 0, 0);

	m_perFrameCBuffers.DirectionalLight.LightDirection = XMFLOAT3(0.2f, -0.9f, -0.3f);

	return true;
}

void SceneBistro::UnloadContent()
{
	Scene::UnloadContent();
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
		SettingsManager::ms_Dynamic.FullscreenEnabled = !SettingsManager::ms_Dynamic.FullscreenEnabled;
		m_pWindow->SetFullscreen(SettingsManager::ms_Dynamic.FullscreenEnabled);
	}

	if (InputManager::IsKeyDown(KeyCode::V))
	{
		SettingsManager::ms_Dynamic.VSyncEnabled = !SettingsManager::ms_Dynamic.VSyncEnabled;
	}

	m_goSkybox->SetPosition(m_camera->GetWorldPosition());
}

void SceneBistro::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);
}
