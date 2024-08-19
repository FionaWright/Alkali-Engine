#include "pch.h"
#include "SceneCube.h"
#include "ImGUIManager.h"
#include "ModelLoaderObj.h"
#include "ModelLoaderGLTF.h"
#include "ResourceTracker.h"
#include "Utils.h"
#include "TextureLoader.h"
#include "RootSig.h"
#include "AssetFactory.h"

SceneCube::SceneCube(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
{
}

bool SceneCube::LoadContent()
{
	Scene::LoadContent();

	CommandQueue* cmdQueueDirect = nullptr;
	cmdQueueDirect = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!cmdQueueDirect)
		throw std::exception("Command Queue Error");
	auto cmdListDirect = cmdQueueDirect->GetAvailableCommandList();	

	RootParamInfo rootParamInfoShrimple;
	rootParamInfoShrimple.NumCBV_PerDraw = 1;
	rootParamInfoShrimple.ParamIndexCBV_PerDraw = 0;

	auto rootSigShrimple = std::make_shared<RootSig>();
	rootSigShrimple->Init("Shrimple Root Sig", rootParamInfoShrimple);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutPBR =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	ShaderArgs argsShrimple = { L"Shrimple_VS.hlsl", L"Shrimple_PS.hlsl", inputLayoutPBR, rootSigShrimple->GetRootSigResource() };
	shared_ptr<Shader> shaderShrimple = AssetFactory::CreateShader(argsShrimple);

	shared_ptr<Batch> batchPBR = AssetFactory::CreateBatch(rootSigShrimple);

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
	shared_ptr<Material> mat = AssetFactory::CreateMaterial();
	mat->AddCBVs(m_d3dClass, cmdListDirect.Get(), cbvSizesDraw, false);

	shared_ptr<Model> modelCube = AssetFactory::CreateModel("Cube.model");

	auto go = batchPBR->CreateGameObject("Cube", modelCube, shaderShrimple, mat);
	go->SetPosition(0, 0, 5);

	auto fenceValue = cmdQueueDirect->ExecuteCommandList(cmdListDirect);
	cmdQueueDirect->WaitForFenceValue(fenceValue);

	m_perFrameCBuffers.DirectionalLight.LightDirection = XMFLOAT3(1, -1, 0);

	return true;
}

void SceneCube::UnloadContent()
{
	Scene::UnloadContent();
}

void SceneCube::OnUpdate(TimeEventArgs& e)
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
}

void SceneCube::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);
}