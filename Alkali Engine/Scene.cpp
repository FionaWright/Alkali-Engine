#include "pch.h"
#include "Scene.h"
#include "ImGUIManager.h"
#include <locale>
#include <codecvt>
#include "Utils.h"

shared_ptr<Model> Scene::ms_sphereModel;
bool Scene::ms_sphereMode;

Scene::Scene(const std::wstring& name, Window* pWindow, bool createDSV)
	: m_Name(name)
	, m_pWindow(pWindow)
	, m_dsvEnabled(createDSV)
	, m_camera(std::make_unique<Camera>(CameraMode::CAMERA_MODE_SCROLL))
	, m_viewMatrix(XMMatrixIdentity())
	, m_projectionMatrix(XMMatrixIdentity())
	, m_viewProjMatrix(XMMatrixIdentity())
	, m_FoV(45.0f)
{
	SetBackgroundColor(0.4f, 0.6f, 0.9f, 1.0f);
}

Scene::~Scene()
{
}

bool Scene::Init(D3DClass* pD3DClass)
{
    // Check for DirectX Math library support.
	// Note: Should this be moved to Application/D3DClass?
    if (!DirectX::XMVerifyCPUSupport())
    {
        MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

	m_d3dClass = pD3DClass;

	m_scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

	float width = m_pWindow->GetClientWidth();
	float height = m_pWindow->GetClientHeight();
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

	if (m_dsvEnabled)
		SetDSVForSize(width, height);

    return true;
}

bool Scene::LoadContent()
{
	{
		auto commandQueueCopy = m_d3dClass->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

		ms_sphereModel = std::make_shared<Model>();
		ms_sphereModel->Init(commandListCopy.Get(), L"Sphere.model");

		auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
		commandQueueCopy->WaitForFenceValue(fenceValue);
	}

	{
		const int paramCount = 1;
		CD3DX12_ROOT_PARAMETER1 rootParameters[paramCount];
		rootParameters[0].InitAsConstants(sizeof(MatricesLineCB) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC sampler[1];
		sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler[0].MipLODBias = 0;
		sampler[0].MaxAnisotropy = 0;
		sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler[0].MinLOD = 0.0f;
		sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
		sampler[0].ShaderRegister = 0;
		sampler[0].RegisterSpace = 0;
		sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		m_rootSigLine = ResourceManager::CreateRootSignature(rootParameters, paramCount, sampler, 1);
		m_rootSigLine->SetName(L"Line Root Sig");
	}

	{
		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_shaderLine = std::make_shared<Shader>();
		m_shaderLine->InitPreCompiled(L"Line_VS.cso", L"Line_PS.cso", inputLayout, _countof(inputLayout), m_rootSigLine.Get(), m_d3dClass->GetDevice(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	}

	vector<DebugLine*> frustumDebugLines;
	for (int i = 0; i < 12; i++)
		frustumDebugLines.push_back(AddDebugLine(XMFLOAT3_ZERO, XMFLOAT3_ZERO, XMFLOAT3_ONE));

	m_frustum.SetDebugLines(frustumDebugLines);

    return true;
}

void Scene::UnloadContent()
{
	SetBackgroundColor(0.4f, 0.6f, 0.9f, 1.0f);
	m_gameObjectList.clear();
	m_camera->Reset();

	if (ms_sphereModel)
	{
		ms_sphereModel.reset();
	}
}

void Scene::Destroy()
{
}

void Scene::OnUpdate(TimeEventArgs& e)
{
	m_camera->Update(e);

	m_viewMatrix = m_camera->GetViewMatrix();
	m_projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
	m_viewProjMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);

	if (!m_freezeFrustum)
		m_frustum.UpdateValues(m_viewProjMatrix);
	
	bool showLines = PERMA_FRUSTUM_DEBUG_LINES || m_freezeFrustum;
	if (showLines)
		m_frustum.CalculateDebugLinePoints(m_d3dClass);

	m_frustum.SetDebugLinesEnabled(showLines);
}

void Scene::OnRender(TimeEventArgs& e)
{
    if (ImGui::CollapsingHeader("Settings"))
    {
		ImGui::Indent(IM_GUI_INDENTATION);

        if (ImGui::TreeNode("Engine##2"))
        {		
            ImGui::SeparatorText("DX12");
			ImGui::Indent(IM_GUI_INDENTATION);

			bool vSync = m_pWindow->IsVSync();
            ImGui::Checkbox("VSync", &vSync);
			m_pWindow->SetVSync(vSync);

			ImGui::Checkbox("Wireframe", &Shader::ms_FillWireframeMode);

			ImGui::Checkbox("Don't Cull Backfaces", &Shader::ms_CullNone);

			ImGui::Checkbox("Freeze Frustum Culling", &m_freezeFrustum);

			ImGui::Checkbox("Bounding Spheres", &ms_sphereMode);

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

	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Current Scene"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		if (ImGui::CollapsingHeader("GameObjects##2"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			if (ImGui::CollapsingHeader("Camera"))
			{
				ImGui::Indent(IM_GUI_INDENTATION);

				ImGui::SeparatorText("Camera Controls");
				ImGui::Indent(IM_GUI_INDENTATION);

				bool usingFP = m_camera->GetMode() == CameraMode::CAMERA_MODE_FP;
				if (!usingFP)
					ImGui::BeginDisabled(true);

				if (ImGui::Button("Scroll Mode"))
				{
					m_camera->SetMode(CameraMode::CAMERA_MODE_SCROLL);
				}

				if (!usingFP)
					ImGui::EndDisabled();

				if (usingFP)
					ImGui::BeginDisabled(true);

				if (ImGui::Button("WASD Mode"))
				{
					m_camera->SetMode(CameraMode::CAMERA_MODE_FP);
				}

				if (usingFP)
					ImGui::EndDisabled();

				ImGui::Spacing();
				ImGui::Unindent(IM_GUI_INDENTATION);

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

					string iStr = "##" + std::to_string(i);

					float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
					ImGui::InputFloat3(("Position" + iStr).c_str(), pPos);
					float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
					ImGui::InputFloat3(("Rotation" + iStr).c_str(), pRot);
					float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
					ImGui::InputFloat3(("Scale" + iStr).c_str(), pScale);

					t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
					t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
					t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

					m_gameObjectList.at(i)->SetTransform(t);

					if (ImGui::TreeNode(("Model Info" + iStr).c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						string vCountStr = "Model Vertex Count: " + std::to_string(m_gameObjectList.at(i)->GetModelVertexCount());
						string iCountStr = "Model Index Count: " + std::to_string(m_gameObjectList.at(i)->GetModelIndexCount());

						XMFLOAT3 pos;
						float radius;
						m_gameObjectList.at(i)->GetBoundingSphere(pos, radius);
						string radiStr = "Model Bounding Radius: " + std::to_string(radius);
						string centroidStr = "Model Centroid: " + ToString(pos);

						ImGui::Text(vCountStr.c_str());
						ImGui::Text(iCountStr.c_str());
						ImGui::Text(radiStr.c_str());
						ImGui::Text(centroidStr.c_str());

						ImGui::TreePop();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					if (ImGui::TreeNode(("Shader Info" + iStr).c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						wstring vs, ps, hs, ds;
						m_gameObjectList.at(i)->GetShaderNames(vs, ps, hs, ds);
						vs = L"VS: " + vs;
						ps = L"PS: " + ps;
						hs = L"HS: " + hs;
						ds = L"DS: " + ds;
						ImGui::Text(wstringToString(vs).c_str());
						ImGui::Text(wstringToString(ps).c_str());
						ImGui::Text(wstringToString(hs).c_str());
						ImGui::Text(wstringToString(ds).c_str());

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
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

	if (m_dsvEnabled)
		SetDSVForSize(e.Width, e.Height);
}

void Scene::OnWindowDestroy()
{
}

bool Scene::IsSphereModeOn(Model*& model)
{
	model = ms_sphereModel.get();
	return ms_sphereMode;
}

Window* Scene::GetWindow()
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

void Scene::ClearBackBuffer(ID3D12GraphicsCommandList2* commandList)
{
	auto backBuffer = m_pWindow->GetCurrentBackBuffer();
	auto rtv = m_pWindow->GetCurrentRenderTargetView();
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	ResourceManager::TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->ClearRenderTargetView(rtv, m_backgroundColor, 0, nullptr);
	ClearDepth(commandList, dsv);
}

void Scene::ClearDepth(ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void Scene::Present(ID3D12GraphicsCommandList2* commandList, CommandQueue* commandQueue)
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
	if (!m_dsvEnabled)
		return;

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
	optimizedClearValue.Format = DSV_FORMAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto dsvResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DSV_FORMAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &dsvResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&m_depthBuffer));
	ThrowIfFailed(hr);

	// Update the depth-stencil view.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DSV_FORMAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE heapStartHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(m_depthBuffer.Get(), &dsv, heapStartHandle);
}

DebugLine* Scene::AddDebugLine(XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color)
{
	auto line = std::make_shared<DebugLine>(m_d3dClass, m_shaderLine, start, end, color);
	m_debugLineList.push_back(line);
	return line.get();
}

void Scene::RenderDebugLines(ID3D12GraphicsCommandList2* commandListDirect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	for (int i = 0; i < m_debugLineList.size(); i++)
	{
		m_debugLineList.at(i)->Render(commandListDirect, m_rootSigLine.Get(), m_viewport, m_scissorRect, rtv, dsv, m_viewProjMatrix);
	}
}
