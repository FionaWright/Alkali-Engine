#include "pch.h"
#include "Tutorial2.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "ResourceTracker.h"
#include "Utils.h"
#include "TextureLoader.h"

Tutorial2::Tutorial2(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)	
{
}

bool Tutorial2::LoadContent()
{
	Scene::LoadContent();

	// Models
	shared_ptr<Model> modelSphere, modelPlane, modelInvertedCube;
	{
		CommandQueue* commandQueueCopy = nullptr;
		commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		auto sphereList = ModelLoader::LoadModelsFromGLTF(m_d3dClass, commandListCopy.Get(), "Sphere.gltf");
		modelSphere = sphereList.at(0);

		string modelName = "Plane.model";
		if (!ResourceTracker::TryGetModel(modelName, modelPlane))
		{
			modelPlane->Init(commandListCopy.Get(), modelName);
		}

		modelName = "Cube (Inverted).model";
		if (!ResourceTracker::TryGetModel(modelName, modelInvertedCube))
		{
			modelInvertedCube->Init(commandListCopy.Get(), modelName);
		}

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");
	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	// Textures
	shared_ptr<Texture> baseTex, normalTex, specTex, skyboxTex, irradianceTex;
	{
		string texName = "EarthDay.png";
		if (!ResourceTracker::TryGetTexture(texName, baseTex))
		{
			//m_texture->Init(m_d3dClass, commandListDirect.Get(), "Bistro/Pavement_Cobblestone_Big_BLENDSHADER_BaseColor.dds");
			baseTex->Init(m_d3dClass, commandListDirect.Get(), texName);
		}	

		string normalName = "EarthNormal.png";
		if (!ResourceTracker::TryGetTexture(normalName, normalTex))
		{
			normalTex->Init(m_d3dClass, commandListDirect.Get(), normalName, false, true);
		}

		string specName = "DefaultSpecular.png";
		if (!ResourceTracker::TryGetTexture(specName, specTex))
		{
			specTex->Init(m_d3dClass, commandListDirect.Get(), specName);
		}

		vector<string> skyboxPaths = {
			"Skyboxes/Iceland/negx.tga",
			"Skyboxes/Iceland/posx.tga",
			"Skyboxes/Iceland/posy.tga",
			"Skyboxes/Iceland/negy.tga",
			"Skyboxes/Iceland/negz.tga",
			"Skyboxes/Iceland/posz.tga"
		};

		if (!ResourceTracker::TryGetTexture(skyboxPaths, skyboxTex))
		{
			skyboxTex->InitCubeMap(m_d3dClass, commandListDirect.Get(), skyboxPaths);
		}

		//string skyboxPath = "Skyboxes/Bistro_Bridge.hdr";
		//if (!ResourceTracker::TryGetTexture(skyboxPath, skyboxTex))
		//{
		//	skyboxTex->InitCubeMapHDR(m_d3dClass, commandListDirect.Get(), skyboxPath);			
		//}

		irradianceTex = std::make_shared<Texture>();
		irradianceTex->InitCubeMapUAV_Empty(m_d3dClass);
		TextureLoader::CreateIrradianceMap(m_d3dClass, commandListDirect.Get(), skyboxTex->GetResource(), irradianceTex->GetResource());
	}

	RootParamInfo rootParamInfoPBR;
	rootParamInfoPBR.NumCBV_PerFrame = 2;
	rootParamInfoPBR.NumCBV_PerDraw = 2;
	rootParamInfoPBR.NumSRV = 4;
	rootParamInfoPBR.ParamIndexCBV_PerDraw = 0;
	rootParamInfoPBR.ParamIndexCBV_PerFrame = 1;
	rootParamInfoPBR.ParamIndexSRV = 2;

	ComPtr<ID3D12RootSignature> rootSigPBR;
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];		
		int shaderRegisterFrameStart = rootParamInfoPBR.NumCBV_PerDraw;
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rootParamInfoPBR.NumCBV_PerDraw, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rootParamInfoPBR.NumCBV_PerFrame, shaderRegisterFrameStart, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rootParamInfoPBR.NumSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			
		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[rootParamInfoPBR.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[rootParamInfoPBR.ParamIndexCBV_PerFrame].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[rootParamInfoPBR.ParamIndexSRV].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

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
		rootSigPBR->SetName(L"Tutorial2 Root Sig");
	}

	RootParamInfo rootParamInfoSkybox;
	rootParamInfoSkybox.NumCBV_PerDraw = 1;
	rootParamInfoSkybox.NumSRV = 1;
	rootParamInfoSkybox.ParamIndexCBV_PerDraw = 0;
	rootParamInfoSkybox.ParamIndexSRV = 1;

	ComPtr<ID3D12RootSignature> rootSigSkybox;
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[rootParamInfoSkybox.ParamIndexCBV_PerDraw].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rootParamInfoSkybox.NumCBV_PerDraw, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[rootParamInfoSkybox.ParamIndexSRV].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rootParamInfoSkybox.NumSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[rootParamInfoSkybox.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &ranges[rootParamInfoSkybox.ParamIndexCBV_PerDraw], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[rootParamInfoSkybox.ParamIndexSRV].InitAsDescriptorTable(1, &ranges[rootParamInfoSkybox.ParamIndexSRV], D3D12_SHADER_VISIBILITY_PIXEL);

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

		rootSigSkybox = ResourceManager::CreateRootSignature(rootParameters, _countof(rootParameters), sampler, _countof(sampler));
		rootSigSkybox->SetName(L"Skybox RootSig");
	}

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB), sizeof(MaterialPropertiesCB) };
	vector<UINT> cbvSizesFrame = { sizeof(CameraCB), sizeof(DirectionalLightCB) };
	vector<shared_ptr<Texture>> textures = { baseTex, normalTex, specTex, irradianceTex };

	shared_ptr<Material> matPBR1 = std::make_shared<Material>();	
	matPBR1->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
	matPBR1->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesFrame, true);
	matPBR1->AddSRVs(m_d3dClass, textures);
	ResourceTracker::AddMaterial(matPBR1);

	shared_ptr<Material> matPBR2 = std::make_shared<Material>();
	matPBR2->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
	matPBR2->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesFrame, true);
	matPBR2->AddSRVs(m_d3dClass, textures);
	ResourceTracker::AddMaterial(matPBR2);

	MaterialPropertiesCB defaultMatProps;
	//defaultMatProps.Roughness = 0;
	//defaultMatProps.Metallic = 1;
	matPBR1->SetCBV_PerDraw(1, &defaultMatProps, sizeof(MaterialPropertiesCB));
	matPBR2->SetCBV_PerDraw(1, &defaultMatProps, sizeof(MaterialPropertiesCB));

	cbvSizesDraw = { sizeof(MatricesCB) };
	textures = { skyboxTex };
	shared_ptr matSkybox = std::make_shared<Material>();
	matSkybox->AddCBVs(m_d3dClass, commandListDirect.Get(), cbvSizesDraw, false);
	matSkybox->AddSRVs(m_d3dClass, textures);
	ResourceTracker::AddMaterial(matSkybox);

	// Shaders
	shared_ptr<Shader> shaderPBR, shaderPBRCullOff, shaderSkybox;
	if (!ResourceTracker::TryGetShader("PBR.vs - PBR.ps", shaderPBR))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		shaderPBR->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSigPBR.Get(), m_d3dClass->GetDevice());
	}

	if (!ResourceTracker::TryGetShader("PBR.vs - PBR.ps --CullOff", shaderPBRCullOff))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		shaderPBRCullOff->Init(L"PBR.vs", L"PBR.ps", inputLayout, rootSigPBR.Get(), m_d3dClass->GetDevice(), true);
	}

	if (!ResourceTracker::TryGetShader("Skybox_VS.cso - Skybox_PS.cso", shaderSkybox))
	{
		vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		shaderSkybox->InitPreCompiled(L"Skybox_VS.cso", L"Skybox_PS.cso", inputLayout, rootSigSkybox.Get(), m_d3dClass->GetDevice(), false, false, true);
	}

	Scene::AddDebugLine(XMFLOAT3(-999, 0, 0), XMFLOAT3(999, 0, 0), XMFLOAT3(1, 0, 0));
	Scene::AddDebugLine(XMFLOAT3(0, -999, 0), XMFLOAT3(0, 999, 0), XMFLOAT3(0, 1, 0));
	Scene::AddDebugLine(XMFLOAT3(0, 0, -999), XMFLOAT3(0, 0, 999), XMFLOAT3(0, 0, 1));

	shared_ptr<Batch> batchPBR, batchSkybox;
	if (!ResourceTracker::TryGetBatch("PBR Basic", batchPBR))
	{
		batchPBR->Init("PBR Basic", rootSigPBR);
	}

	if (!ResourceTracker::TryGetBatch("Skybox", batchSkybox))
	{
		batchSkybox->Init("Skybox", rootSigSkybox);
	}

	Transform t = { XMFLOAT3(0, 9, 0), XMFLOAT3_ZERO, XMFLOAT3_ONE };

	vector<string> whiteList = { "Bistro_Research_Exterior_Paris_Street_" };
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", rootParamInfoPBR, batchPBR.get(), shaderPBR, irradianceTex, shaderPBRCullOff, &whiteList);
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "MetalRoughSpheres.gltf", rootParamInfoPBR, batchPBR.get(), shaderPBR, irradianceTex, shaderPBRCullOff, nullptr, t);
	//ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Primitives.glb", rootParamInfo, batch.get(), shaderPBR);

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderCube);
	//m_batch->AddHeldGameObjectsToList(m_gameObjectList);

	GameObject go("Test", rootParamInfoPBR, modelSphere, shaderPBR, matPBR1);
	go.SetPosition(-50, 3, -10);
	go.SetScale(20);
	m_goTest = batchPBR->AddGameObject(go);

	//GameObject go2("Plane", rootParamInfo, modelPlane, shaderPBRCullOff, matPBR2);
	//m_goPlane = batch->AddGameObject(go2);
	//m_goPlane->SetPosition(3, 3, 0);

	GameObject goSkybox("Skybox", rootParamInfoSkybox, modelInvertedCube, shaderSkybox, matSkybox);
	goSkybox.SetScale(20);
	m_goSkybox = batchSkybox->AddGameObject(goSkybox);

	m_camera->SetPosition(16, 6, -5);
	m_camera->SetRotation(0, -90, 0);

	auto fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

	return true;
}

void Tutorial2::UnloadContent()
{
	Scene::UnloadContent();

	m_FoV = 45;
}

void Tutorial2::OnUpdate(TimeEventArgs& e)
{
	Scene::OnUpdate(e);

	XMFLOAT2 mousePos = InputManager::GetMousePos();

	float angle = static_cast<float>(e.TotalTime * 2.0f);
	//m_goCube->RotateBy(0, angle, 0);
	m_perFrameCBuffers.DirectionalLight.LightDirection = Normalize(XMFLOAT3(cos(angle), -0.5f, sin(angle)));

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

void Tutorial2::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);	
}