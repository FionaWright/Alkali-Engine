#include "pch.h"
#include "Tutorial2.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"
#include "ResourceTracker.h"

Tutorial2::Tutorial2(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)	
{
}

bool Tutorial2::LoadContent()
{
	Scene::LoadContent();

	// Models
	shared_ptr<Model> modelSphere, modelPlane;
	{
		CommandQueue* commandQueueCopy = nullptr;
		commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		auto sphereList = ModelLoader::LoadModelsFromGLTF(m_d3dClass, commandListCopy.Get(), "Sphere.gltf");
		modelSphere = sphereList.at(0);

		string modelName2 = "Plane.model";
		if (!ResourceTracker::TryGetModel(modelName2, modelPlane))
		{
			modelPlane->Init(commandListCopy.Get(), modelName2);
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
	shared_ptr<Texture> baseTex, normalTex;
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
	}

	ComPtr<ID3D12RootSignature> rootSigPBR;
	int numCBV = 3, numSRV = 2;
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];		
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, numCBV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsDescriptorTable(_countof(ranges), &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

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

	string matID = baseTex->GetFilePath() + " - " + normalTex->GetFilePath();
	shared_ptr<Material> matPBR1, matPBR2;
	if (!ResourceTracker::TryGetMaterial(matID, matPBR1))
	{
		matPBR1->Init(numSRV, numCBV);
		matPBR1->AddCBuffer(m_d3dClass, commandListDirect.Get(), sizeof(MatricesCB));
		matPBR1->AddCBuffer(m_d3dClass, commandListDirect.Get(), sizeof(CameraCB));
		matPBR1->AddCBuffer(m_d3dClass, commandListDirect.Get(), sizeof(DirectionalLightCB));
		matPBR1->AddTexture(m_d3dClass, baseTex);
		matPBR1->AddTexture(m_d3dClass, normalTex);
	}

	matPBR2 = std::make_shared<Material>();
	matPBR2->Init(numSRV, numCBV);
	matPBR2->AddCBuffer(m_d3dClass, commandListDirect.Get(), sizeof(MatricesCB));
	matPBR2->AddCBuffer(m_d3dClass, commandListDirect.Get(), sizeof(CameraCB));
	matPBR2->AddCBuffer(m_d3dClass, commandListDirect.Get(), sizeof(DirectionalLightCB));
	matPBR2->AddTexture(m_d3dClass, baseTex);
	matPBR2->AddTexture(m_d3dClass, normalTex);

	// Shaders
	shared_ptr<Shader> shaderPBR, shaderPBRCullOff;
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
		//m_shaderCube->InitPreCompiled(L"Test_VS.cso", L"Test_PS.cso", inputLayout, _countof(inputLayout), rootSig);
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

	Scene::AddDebugLine(XMFLOAT3(-999, 0, 0), XMFLOAT3(999, 0, 0), XMFLOAT3(1, 0, 0));
	Scene::AddDebugLine(XMFLOAT3(0, -999, 0), XMFLOAT3(0, 999, 0), XMFLOAT3(0, 1, 0));
	Scene::AddDebugLine(XMFLOAT3(0, 0, -999), XMFLOAT3(0, 0, 999), XMFLOAT3(0, 0, 1));

	shared_ptr<Batch> batch;
	if (!ResourceTracker::TryGetBatch("PBR Basic", batch))
	{
		batch->Init("PBR Basic", rootSigPBR);
	}

	vector<string> whiteList = { "Bistro_Research_Exterior_Paris_Street_" };
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", batch.get(), shaderPBR, shaderPBRCullOff, &whiteList);
	ModelLoader::LoadSplitModelGLTF(m_d3dClass, commandListDirect.Get(), "Primitives.glb", batch.get(), shaderPBR);

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderCube);
	//m_batch->AddHeldGameObjectsToList(m_gameObjectList);

	GameObject go("Test", modelSphere, shaderPBR, matPBR1);
	go.SetPosition(-50, 3, -10);
	go.SetScale(20);
	m_goTest = batch->AddGameObject(go);

	GameObject go2("Plane", modelPlane, shaderPBRCullOff, matPBR2);
	m_goPlane = batch->AddGameObject(go2);
	m_goPlane->SetPosition(3, 3, 0);

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
	m_perFrameCBuffers.DirectionalLight.LightDirection = XMFLOAT3(cos(angle), -0.5f, sin(angle));

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

void Tutorial2::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);	
}