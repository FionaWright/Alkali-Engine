#include "pch.h"
#include "Tutorial2.h"

Tutorial2::Tutorial2(const std::wstring& name, int width, int height, bool vSync) 
	: super(name, width, height, vSync, true)
	, m_FoV(45.0f)
	, m_ViewMatrix(XMMatrixIdentity())
	, m_ProjectionMatrix(XMMatrixIdentity())	
	, m_modelCube(std::make_shared<Model>())
	, m_modelMadeline(std::make_shared<Model>())
	, m_shaderCube(std::make_shared<Shader>())
{
}

bool Tutorial2::LoadContent()
{
	auto commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList = commandQueue->GetAvailableCommandList();

	m_modelMadeline->Init(commandList, L"Madeline.model");

	// A single 32-bit constant root parameter that is used by the vertex shader.
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	auto rootSig = ResourceManager::CreateRootSignature(rootParameters, 1);

	m_batch = std::make_shared<Batch>(rootSig);

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_shaderCube->Init(L"Test.vs", L"Test.ps", inputLayout, _countof(inputLayout), rootSig, m_d3dClass->GetDevice());
	//m_shaderCube->InitPreCompiled(L"Test_VS.cso", L"Test_PS.cso", inputLayout, _countof(inputLayout), rootSig);

	m_goCube = std::make_shared<GameObject>(m_modelMadeline, m_shaderCube);

	m_batch->AddGameObject(m_goCube.get());

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);
	m_ContentLoaded = true;

	return true;
}

void Tutorial2::UnloadContent()
{
}

void Tutorial2::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	super::OnUpdate(e);

	totalTime += e.ElapsedTime;
	frameCount++;

	if (totalTime > 1.0)
	{
		double fps = frameCount / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}

	// Update the model matrix.
	float angle = static_cast<float>(e.TotalTime * 1.0);
	m_goCube->SetRotation(0, angle, angle);

	// Update the view matrix.
	const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
	const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
	m_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	// Update the projection matrix.
	float aspectRatio = m_width / static_cast<float>(m_height);
	m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), aspectRatio, 0.1f, 100.0f);
}

void Tutorial2::OnRender(RenderEventArgs& e)
{
	super::OnRender(e);

	auto commandQueue = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetAvailableCommandList();
	
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ClearBackBuffer(commandList);

	XMMATRIX viewProj = XMMatrixMultiply(m_ViewMatrix, m_ProjectionMatrix);

	m_batch->Render(commandList, m_viewport, m_scissorRect, rtv, dsv, viewProj);

	Present(commandList, commandQueue);
}

void Tutorial2::OnKeyPressed(KeyEventArgs& e)
{
	super::OnKeyPressed(e);

	if (e.Key == KeyCode::Escape)
	{
		PostQuitMessage(0);
	}

	bool altEnter = e.Key == KeyCode::Enter && e.Alt;
	if (altEnter || e.Key == KeyCode::F11)
	{
		m_pWindow->ToggleFullscreen();
	}

	if (e.Key == KeyCode::V)
	{
		m_pWindow->ToggleVSync();
	}
}

void Tutorial2::OnMouseWheel(MouseWheelEventArgs& e)
{
	m_FoV -= e.WheelDelta;
	m_FoV = std::clamp(m_FoV, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FoV: %f\n", m_FoV);
	OutputDebugStringA(buffer);
}