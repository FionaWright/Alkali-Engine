#include "pch.h"
#include "Scene.h"
#include "ImGUIManager.h"
#include <locale>
#include <codecvt>

Scene::Scene(const std::wstring& name, int width, int height, bool vSync, bool createDSV)
	: m_width(width)
	, m_height(height)
	, m_vSync(vSync)
	, m_Name(name)
	, m_dsvEnabled(createDSV)
	, m_camera(std::make_unique<Camera>(CameraMode::CAMERA_MODE_FP))
{
	SetBackgroundColor(0.4f, 0.6f, 0.9f, 1.0f);
}

Scene::~Scene()
{
}

bool Scene::Init(shared_ptr<D3DClass> pD3DClass)
{
    // Check for DirectX Math library support.
    if (!DirectX::XMVerifyCPUSupport())
    {
        MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

	m_d3dClass = pD3DClass;

    m_pWindow = WindowManager::GetInstance()->CreateRenderWindow(pD3DClass, m_Name, m_width, m_height, m_vSync);
    m_pWindow->RegisterCallbacks(shared_from_this());
    m_pWindow->Show();

	m_scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));	

	if (m_dsvEnabled)
		SetDSVForSize(m_width, m_height);

    return true;
}

bool Scene::LoadContent()
{
    return false;
}

void Scene::UnloadContent()
{
}

void Scene::Destroy()
{
	if (!m_pWindow)
		return;

	m_pWindow->Destroy();
    m_pWindow.reset();
}

void Scene::OnUpdate(UpdateEventArgs& e)
{
	m_camera->Update(e);
}

std::string ws2s(const std::wstring& wstr)
{
	std::string str;
	std::transform(wstr.begin(), wstr.end(), std::back_inserter(str), [](wchar_t c) {
		return (char)c;
		});
	return str;
}

void Scene::OnRender(RenderEventArgs& e)
{
    if (ImGui::CollapsingHeader("Settings"))
    {
		ImGui::Indent(IM_GUI_INDENTATION);

        if (ImGui::TreeNode("Engine##2"))
        {		
            ImGui::SeparatorText("DX12");
			ImGui::Indent(IM_GUI_INDENTATION);

            ImGui::Checkbox("VSync", &m_vSync);    
			m_pWindow->SetVSync(m_vSync);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Window");
			ImGui::Indent(IM_GUI_INDENTATION);

			bool fullScreen = m_pWindow->IsFullScreen();
			ImGui::Checkbox("Fullscreen", &fullScreen);
			m_pWindow->SetFullscreen(fullScreen);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("ImGUI");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::Checkbox("Show demo window", &m_showImGUIDemo);

			ImGui::Unindent(IM_GUI_INDENTATION);
            ImGui::TreePop();
            ImGui::Spacing();			
        }

		if (ImGui::TreeNode("Scene##2"))
		{		
			ImGui::SeparatorText("Visuals");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::ColorEdit4("Background Color", m_backgroundColor, ImGuiColorEditFlags_DisplayHex);			

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Rendering");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::Checkbox("DSV", &m_dsvEnabled);

			ImGui::TreePop();
			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		ImGui::Unindent(IM_GUI_INDENTATION);
    }

	if (ImGui::CollapsingHeader("Scenes"))
	{
		if (ImGui::Button("Reset current scene"))
		{

		}
	}

	if (ImGui::CollapsingHeader("Current Scene"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		if (ImGui::CollapsingHeader("GameObjects##2"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			if (ImGui::CollapsingHeader("Camera"))
			{
				ImGui::Indent(IM_GUI_INDENTATION);

				Transform camTrans = m_camera->GetTransform();
				float pPosCam[3] = { camTrans.Position.x, camTrans.Position.y, camTrans.Position.z };
				ImGui::InputFloat3("Position##Camera", pPosCam);
				float pRotCam[3] = { camTrans.Rotation.x, camTrans.Rotation.y, camTrans.Rotation.z };
				ImGui::InputFloat3("Rotation##Camera", pRotCam);
				camTrans.Position = XMFLOAT3(pPosCam[0], pPosCam[1], pPosCam[2]);
				camTrans.Rotation = XMFLOAT3(pRotCam[0], pRotCam[1], pRotCam[2]);

				if (ImGui::Button("Reset to Default"))
				{
					camTrans.Position = XMFLOAT3(0, 0, -10);
					camTrans.Rotation = XMFLOAT3(0, 0, 0);
				}

				m_camera->SetTransform(camTrans);

				ImGui::Unindent(IM_GUI_INDENTATION);
			}

			for (size_t i = 0; i < m_gameObjectList.size(); i++)
			{
				if (ImGui::CollapsingHeader(m_gameObjectList.at(i)->m_Name.c_str()))
				{
					ImGui::Indent(IM_GUI_INDENTATION);

					Transform t = m_gameObjectList.at(i)->GetTransform();

					float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
					ImGui::InputFloat3("Position##" + i, pPos);
					float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
					ImGui::InputFloat3("Rotation##" + i, pRot);
					float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
					ImGui::InputFloat3("Scale##" + i, pScale);

					t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
					t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
					t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

					m_gameObjectList.at(i)->SetTransform(t);

					if (ImGui::TreeNode("Model Info"))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						string vCountStr = "Model Vertex Count: " + std::to_string(m_gameObjectList.at(i)->GetModelVertexCount());
						string iCountStr = "Model Index Count: " + std::to_string(m_gameObjectList.at(i)->GetModelIndexCount());

						ImGui::Text(vCountStr.c_str());
						ImGui::Text(iCountStr.c_str());

						ImGui::TreePop();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					if (ImGui::TreeNode("Shader Info"))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						wstring vs, ps, hs, ds;
						m_gameObjectList.at(i)->GetShaderNames(vs, ps, hs, ds);
						vs = L"VS: " + vs;
						ps = L"PS: " + ps;
						hs = L"HS: " + hs;
						ds = L"DS: " + ds;
						ImGui::Text(ws2s(vs).c_str());
						ImGui::Text(ws2s(ps).c_str());
						ImGui::Text(ws2s(hs).c_str());
						ImGui::Text(ws2s(ds).c_str());

						ImGui::TreePop();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					ImGui::Spacing();
					ImGui::Unindent(IM_GUI_INDENTATION);
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	if (m_showImGUIDemo)
		ImGui::ShowDemoWindow(); // Show demo window! :)
}

void Scene::OnResize(ResizeEventArgs& e)
{
	if (e.Width == m_width && e.Height == m_height)
		return;

	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

	if (m_dsvEnabled)
		SetDSVForSize(e.Width, e.Height);
}

void Scene::OnWindowDestroy()
{
}

shared_ptr<Window> Scene::GetWindow()
{
	return m_pWindow;
}

void Scene::SetBackgroundColor(float r, float g, float b, float a)
{
	m_backgroundColor[0] = r;
	m_backgroundColor[1] = g;
	m_backgroundColor[2] = b;
	m_backgroundColor[3] = a;
}

void Scene::ClearBackBuffer(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->ClearRenderTargetView(rtv, m_backgroundColor, 0, nullptr);
	ClearDepth(commandList, dsv);
}

void Scene::ClearDepth(ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void Scene::Present(ComPtr<ID3D12GraphicsCommandList2> commandList, shared_ptr<CommandQueue> commandQueue)
{
	UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	m_FenceValues.at(currentBackBufferIndex) = commandQueue->ExecuteCommandList(commandList);

	UINT nextBackBufferIndex = m_pWindow->Present();

	commandQueue->WaitForFenceValue(m_FenceValues.at(nextBackBufferIndex));
}

void Scene::SetDSVForSize(int width, int height)
{
	HRESULT hr;

	auto device = m_d3dClass->GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
	ThrowIfFailed(hr);

	m_d3dClass->Flush();

	width = std::max(1, width);
	height = std::max(1, height);

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&m_depthBuffer));
	ThrowIfFailed(hr);

	// Update the depth-stencil view.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE heapStartHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(m_depthBuffer.Get(), &dsv, heapStartHandle);
}
