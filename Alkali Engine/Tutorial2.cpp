#include "pch.h"
#include "Tutorial2.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "ResourceTracker.h"
#include "Utils.h"
#include "TextureLoader.h"
#include "RootSig.h"

Tutorial2::Tutorial2(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)	
{
}

bool Tutorial2::LoadContent()
{
	Scene::LoadContent();

	shared_ptr<Model> modelSphere, modelPlane, modelInvertedCube;
	{
		CommandQueue* commandQueueCopy = nullptr;
		commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		auto sphereList = ModelLoader::LoadModelsFromGLTF(m_d3dClass, commandListCopy.Get(), "Sphere.gltf");
		modelSphere = sphereList.at(0);

		modelPlane = CreateModel("Plane.model", commandListCopy.Get());
		modelInvertedCube = CreateModel("Cube (Inverted).model", commandListCopy.Get());

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");
	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	shared_ptr<Texture> baseTex = CreateTexture("EarthDay.png", commandListDirect.Get());
	shared_ptr<Texture> normalTex = CreateTexture("EarthNormal.png", commandListDirect.Get(), false, true);
	shared_ptr<Texture> specTex = CreateTexture("DefaultSpecular.png", commandListDirect.Get());
	shared_ptr<Texture> skyboxTex = CreateCubemapHDR("Skyboxes/Bistro_Bridge.hdr", commandListDirect.Get());
	shared_ptr<Texture> irradianceTex = CreateIrradianceMap(skyboxTex.get(), commandListDirect.Get());

	//vector<string> skyboxPaths = {
	//	"Skyboxes/Iceland/negx.tga",
	//	"Skyboxes/Iceland/posx.tga",
	//	"Skyboxes/Iceland/posy.tga",
	//	"Skyboxes/Iceland/negy.tga",
	//	"Skyboxes/Iceland/negz.tga",
	//	"Skyboxes/Iceland/posz.tga"
	//};

	RootParamInfo rootParamInfoPBR;
	rootParamInfoPBR.NumCBV_PerFrame = 2;
	rootParamInfoPBR.NumCBV_PerDraw = 2;
	rootParamInfoPBR.NumSRV = 5;
	rootParamInfoPBR.ParamIndexCBV_PerDraw = 0;
	rootParamInfoPBR.ParamIndexCBV_PerFrame = 1;
	rootParamInfoPBR.ParamIndexSRV = 2;	

	auto rootSigPBR = std::make_shared<RootSig>();
	rootSigPBR->InitDefaultSampler("PBR Root Sig", rootParamInfoPBR);

	RootParamInfo rootParamInfoSkybox;
	rootParamInfoSkybox.NumCBV_PerDraw = 1;
	rootParamInfoSkybox.NumSRV = 1;
	rootParamInfoSkybox.ParamIndexCBV_PerDraw = 0;
	rootParamInfoSkybox.ParamIndexSRV = 1;

	auto rootSigSkybox = std::make_shared<RootSig>();
	rootSigSkybox->InitDefaultSampler("Skybox Root Sig", rootParamInfoSkybox);

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB), sizeof(MaterialPropertiesCB) };
	vector<UINT> cbvSizesFrame = { sizeof(CameraCB), sizeof(DirectionalLightCB) };
	vector<shared_ptr<Texture>> textures = { baseTex, normalTex, specTex, irradianceTex, skyboxTex };

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
	matPBR1->SetCBV_PerDraw(1, &defaultMatProps, sizeof(MaterialPropertiesCB));
	matPBR2->SetCBV_PerDraw(1, &defaultMatProps, sizeof(MaterialPropertiesCB));

	cbvSizesDraw = { sizeof(MatricesCB) };
	textures = { skyboxTex };

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

	Scene::AddDebugLine(XMFLOAT3(-999, 0, 0), XMFLOAT3(999, 0, 0), XMFLOAT3(1, 0, 0));
	Scene::AddDebugLine(XMFLOAT3(0, -999, 0), XMFLOAT3(0, 999, 0), XMFLOAT3(0, 1, 0));
	Scene::AddDebugLine(XMFLOAT3(0, 0, -999), XMFLOAT3(0, 0, 999), XMFLOAT3(0, 0, 1));

	shared_ptr<Batch> batchPBR = CreateBatch(rootSigPBR);
	shared_ptr<Batch> batchSkybox = CreateBatch(rootSigSkybox);

	Transform t = { XMFLOAT3(0, 9, 0), XMFLOAT3_ZERO, XMFLOAT3_ONE };

	vector<string> whiteList = { "Bistro_Research_Exterior_Paris_Street_" };
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", rootParamInfoPBR, batchPBR.get(), skyboxTex, irradianceTex, shaderPBR, shaderPBRCullOff, &whiteList);
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "MetalRoughSpheres.gltf", rootParamInfoPBR, batchPBR.get(), skyboxTex, irradianceTex, shaderPBR, shaderPBRCullOff, nullptr, t);

	m_goTest = batchPBR->CreateGameObject("Test", modelSphere, shaderPBR, matPBR1);
	m_goTest->SetPosition(-50, 3, -10);
	m_goTest->SetScale(20);

	//GameObject go2("Plane", rootParamInfo, modelPlane, shaderPBRCullOff, matPBR2);
	//m_goPlane = batch->AddGameObject(go2);
	//m_goPlane->SetPosition(3, 3, 0);

	m_goSkybox = batchSkybox->CreateGameObject("Skybox", modelInvertedCube, shaderSkybox, matSkybox);
	m_goSkybox->SetScale(20);

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