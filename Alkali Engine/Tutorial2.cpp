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
	{
		CommandQueue* commandQueueCopy = nullptr;
		commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		if (!commandQueueCopy)
			throw std::exception("Command Queue Error");

		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		if (!ResourceTracker::TryGetModel("Sphere.model", m_modelTest))
		{
			m_modelTest->Init(commandListCopy.Get(), L"Sphere.model");
		}
		//m_modelTest->Init(commandListCopy.Get(), L"Bistro/Pavement_Cobblestone_Big_BLENDSHADER.model");		
		//m_modelMadeline->Init(commandListCopy.Get(), L"Bistro/Foliage_Bux_Hedges46.DoubleSided.model");

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");

	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	// Textures
	{
		if (!ResourceTracker::TryGetTexture("Baba.png", m_texture))
		{
			//m_texture->Init(m_d3dClass, commandListDirect.Get(), "Bistro/Pavement_Cobblestone_Big_BLENDSHADER_BaseColor.dds");
			m_texture->Init(m_d3dClass, commandListDirect.Get(), "Baba.png");
		}	

		if (!ResourceTracker::TryGetTexture("Bistro/Pavement_Cobblestone_Big_BLENDSHADER_Normal.dds", m_normalMap))
		{
			m_normalMap->Init(m_d3dClass, commandListDirect.Get(), "Bistro/Pavement_Cobblestone_Big_BLENDSHADER_Normal.dds");
		}
	}

	string matID = m_texture->GetFilePath() + " - " + m_normalMap->GetFilePath();
	if (!ResourceTracker::TryGetMaterial(matID, m_material))
	{
		m_material->Init(2);
		m_material->AddTexture(m_d3dClass, m_texture);
		m_material->AddTexture(m_d3dClass, m_normalMap);
	}

	ComPtr<ID3D12RootSignature> rootSigPBR;
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		int numDescriptors = 2;
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numDescriptors, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

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

		rootSigPBR = ResourceManager::CreateRootSignature(rootParameters, paramCount, sampler, 1);
		rootSigPBR->SetName(L"Tutorial2 Root Sig");
	}

	// Shaders
	if (!ResourceTracker::TryGetShader("PBR.vs - PBR.ps", m_shaderCube))
	{
		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		m_shaderCube->Init(L"PBR.vs", L"PBR.ps", inputLayout, _countof(inputLayout), rootSigPBR.Get(), m_d3dClass->GetDevice());
		//m_shaderCube->InitPreCompiled(L"Test_VS.cso", L"Test_PS.cso", inputLayout, _countof(inputLayout), rootSig);
	}

	Scene::AddDebugLine(XMFLOAT3(-999, 0, 0), XMFLOAT3(999, 0, 0), XMFLOAT3(1, 0, 0));
	Scene::AddDebugLine(XMFLOAT3(0, -999, 0), XMFLOAT3(0, 999, 0), XMFLOAT3(0, 1, 0));
	Scene::AddDebugLine(XMFLOAT3(0, 0, -999), XMFLOAT3(0, 0, 999), XMFLOAT3(0, 0, 1));

	if (!ResourceTracker::TryGetBatch("PBR Basic", m_batch))
	{
		m_batch->Init("PBR Basic", rootSigPBR);
	}

	vector<string> whiteList = { "Bistro_Research_Exterior_Paris_Street_" };
	ModelLoader::LoadModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", m_batch.get(), m_shaderCube, &whiteList);
	ModelLoader::LoadModelGLTF(m_d3dClass, commandListDirect.Get(), "Primitives.glb", m_batch.get(), m_shaderCube);

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderCube);
	//m_batch->AddHeldGameObjectsToList(m_gameObjectList);

	GameObject go("Test", m_modelTest, m_shaderCube, m_material);
	go.SetPosition(0, 10, 0);
	m_goCube = m_batch->AddGameObject(go);

	m_camera->SetPosition(0, 0, -10);
	m_camera->SetRotation(0, 0, 0);

	auto fenceValue = commandQueueDirect->ExecuteCommandList(commandListDirect);
	commandQueueDirect->WaitForFenceValue(fenceValue);

	return true;
}

void Tutorial2::UnloadContent()
{
	Scene::UnloadContent();

	m_FoV = 45;
	m_modelTest.reset();
	m_batch.reset();
	m_shaderCube.reset();
}

void Tutorial2::OnUpdate(TimeEventArgs& e)
{
	Scene::OnUpdate(e);

	XMFLOAT2 mousePos = InputManager::GetMousePos();

	float angle = static_cast<float>(e.ElapsedTime * 50.0);
	//m_goCube->RotateBy(0, angle, 0);

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