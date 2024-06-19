#include "pch.h"
#include "Tutorial2.h"
#include "ImGUIManager.h"
#include "ModelLoader.h"

Tutorial2::Tutorial2(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)	
{
}

bool Tutorial2::LoadContent()
{
	Scene::LoadContent();

	//ModelLoader::PreprocessObjFile("C:\\Users\\finnw\\OneDrive\\Documents\\3D objects\\Cube.obj", false);

	// Models
	{
		auto commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		//ModelLoader::PreprocessObjFile("C:\\Users\\finnw\\OneDrive\\Documents\\3D objects\\Robot.obj", false);
		m_modelTest = std::make_shared<Model>();
		//m_modelTest->Init(commandListCopy.Get(), L"Bistro/Pavement_Cobblestone_Big_BLENDSHADER.model");
		m_modelTest->Init(commandListCopy.Get(), L"Sphere.model");
		//m_modelMadeline->Init(commandListCopy.Get(), L"Bistro/Foliage_Bux_Hedges46.DoubleSided.model");

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	auto commandQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	// Textures
	{
		m_texture = std::make_shared<Texture>();
		m_texture->Init(m_d3dClass, commandListDirect.Get(), "Bistro/Pavement_Cobblestone_Big_BLENDSHADER_BaseColor.dds");
		//m_texture->Init(m_d3dClass, commandListDirect.Get(), "Celeste.tga");

		m_normalMap = std::make_shared<Texture>();
		m_normalMap->Init(m_d3dClass, commandListDirect.Get(), "Bistro/Pavement_Cobblestone_Big_BLENDSHADER_Normal.dds");
	}

	// Materials
	{
		m_material = std::make_shared<Material>(2);
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

		D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		D3D12_STATIC_SAMPLER_DESC sampler[1]; // Change this to return from a texture creation
		sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler[0].AddressU = addressMode;
		sampler[0].AddressV = addressMode;
		sampler[0].AddressW = addressMode;
		sampler[0].MipLODBias = 0;
		sampler[0].MaxAnisotropy = 0;
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
	{
		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		m_shaderCube = std::make_shared<Shader>();
		m_shaderCube->Init(L"PBR.vs", L"PBR.ps", inputLayout, _countof(inputLayout), rootSigPBR.Get(), m_d3dClass->GetDevice());
		//m_shaderCube->InitPreCompiled(L"Test_VS.cso", L"Test_PS.cso", inputLayout, _countof(inputLayout), rootSig);
	}	

	Scene::AddDebugLine(XMFLOAT3(-999, 0, 0), XMFLOAT3(999, 0, 0), XMFLOAT3(1, 0, 0));
	Scene::AddDebugLine(XMFLOAT3(0, -999, 0), XMFLOAT3(0, 999, 0), XMFLOAT3(0, 1, 0));
	Scene::AddDebugLine(XMFLOAT3(0, 0, -999), XMFLOAT3(0, 0, 999), XMFLOAT3(0, 0, 1));

	m_batch = std::make_shared<Batch>(rootSigPBR);

	//ModelLoader::LoadModelGLTF(m_d3dClass, commandListDirect.Get(), "Primitives", m_batch.get(), m_shaderCube);
	vector<string> whiteList = { "Bistro_Research_Exterior_Paris_Street_" };
	ModelLoader::LoadModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro.gltf", m_batch.get(), m_shaderCube, whiteList);
	//ModelLoader::LoadModelGLTF(m_d3dClass, commandListDirect.Get(), "Bistro", m_batch.get(), m_shaderCube);
	m_batch->AddHeldGameObjectsToList(m_gameObjectList); // Shit code, change

	//ModelLoader::LoadSplitModel(m_d3dClass, commandListDirect.Get(), "Madeline", m_batch.get(), m_shaderCube);
	//m_batch->AddHeldGameObjectsToList(m_gameObjectList);

	m_goCube = std::make_shared<GameObject>("Test", m_modelTest, m_shaderCube, m_material);
	m_goCube->SetPosition(0, 10, 0);
	m_gameObjectList.push_back(m_goCube.get());
	m_batch->AddGameObject(m_goCube);

	//shared_ptr<GameObject> refCube = std::make_shared<GameObject>("Ref", m_modelTest, m_shaderCube, m_material);
	//refCube->SetPosition(-3, 0, 0);
	//m_gameObjectList.push_back(refCube.get());
	//m_batch->AddGameObject(refCube);	

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
	m_goCube.reset();	
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

	auto commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetAvailableCommandList();
	
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtvCPUDesc = m_pWindow->GetCurrentRenderTargetView();
	auto dsvCPUDesc = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ClearBackBuffer(commandList.Get());	

	m_batch->Render(commandList.Get(), m_viewport, m_scissorRect, rtvCPUDesc, dsvCPUDesc, m_viewProjMatrix, m_frustum);

	Scene::RenderDebugLines(commandList.Get(), rtvCPUDesc, dsvCPUDesc);

	ImGUIManager::Render(commandList.Get());

	Present(commandList.Get(), commandQueue);
}