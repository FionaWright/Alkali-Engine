#include "pch.h"
#include "AlkaliGUIManager.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"
#include "DescriptorManager.h"
#include "InputManager.h"
#include "ModelLoaderObj.h"
#include "Camera.h"
#include "Scene.h"
#include "Application.h"
#include "AssetFactory.h"
#include <ShadowManager.h>

vector<string> AlkaliGUIManager::ms_errorLog, AlkaliGUIManager::ms_asyncLog;
std::shared_mutex AlkaliGUIManager::ms_asyncLogMutex;

void AlkaliGUIManager::FixWidthOnNext(const char* label) 
{
	ImGui::SetNextItemWidth(-(ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemInnerSpacing.x));
}

void AlkaliGUIManager::RenderGUI(D3DClass* d3d, Scene* scene, Application* app)
{
	ImGui::SeparatorText("Stats");
	ImGui::Indent(IM_GUI_INDENTATION);

	string fpsTxt = "FPS: " + std::to_string(app->GetFPS());
	ImGui::Text(fpsTxt.c_str());

	XMFLOAT2 mousePos = InputManager::GetMousePos();
	string mouseTxt = "Mouse: (" + std::to_string(mousePos.x) + ", " + std::to_string(mousePos.y) + ")";
	ImGui::Text(mouseTxt.c_str());

	XMFLOAT2 mousePosDelta = InputManager::GetMousePosDelta();
	string mouseDeltaTxt = "Mouse delta: (" + std::to_string(mousePosDelta.x) + ", " + std::to_string(mousePosDelta.y) + ")";
	ImGui::Text(mouseDeltaTxt.c_str());

	ImGui::Spacing();
	ImGui::Unindent(IM_GUI_INDENTATION);

	string errorMsg = "Error Log";
	if (ms_errorLog.size() > 0)
		errorMsg += " (!)";

	if (ImGui::TreeNode(errorMsg.c_str()))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		for (size_t i = 0; i < ms_errorLog.size(); i++)
			ImGui::Text(ms_errorLog[i].c_str());

		if (ms_errorLog.size() == 0)
			ImGui::Text("None :)");

		ImGui::Spacing();
		ImGui::Unindent(IM_GUI_INDENTATION);

		ImGui::TreePop();
	}

	if (ms_asyncLogMutex.try_lock_shared())
	{
		string asyncMsg = "Async Log";
		if (ms_asyncLog.size() > 0)
			asyncMsg += " (!)";

		if (ImGui::TreeNode(asyncMsg.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (size_t i = 0; i < ms_asyncLog.size(); i++)
				ImGui::Text(ms_asyncLog[i].c_str());

			if (ms_asyncLog.size() == 0)
				ImGui::Text("None :)");

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);

			ImGui::TreePop();
		}

		ms_asyncLogMutex.unlock_shared();
	}

	RenderGUISettings(d3d, scene);
	RenderGUITools(d3d, scene);
	RenderGUISceneList(d3d, scene, app);
	RenderGUICurrentScene(d3d, scene);

	if (SettingsManager::ms_Dynamic.ShowImGuiDemoWindow)
		ImGui::ShowDemoWindow();
}

void AlkaliGUIManager::LogErrorMessage(string msg)
{
	ms_errorLog.push_back(msg);
}

void AlkaliGUIManager::LogAsyncMessage(string msg)
{
	if (!SettingsManager::ms_DX12.Async.Enabled)
		return;

	if (SettingsManager::ms_DX12.Async.PrintLogIntoConsole)
	{
		wstring wstr(msg.begin(), msg.end());
		OutputDebugString((wstr + L"\n").c_str());
	}

	std::unique_lock<std::shared_mutex> lock(ms_asyncLogMutex);
	ms_asyncLog.push_back(msg);
}

void AlkaliGUIManager::RenderGUISettings(D3DClass* d3d, Scene* scene)
{
	auto& shaderList = ResourceTracker::GetShaders();

	if (ImGui::CollapsingHeader("Settings"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		ImGui::Checkbox("Quick Debug", &SettingsManager::ms_Dynamic.QuickDebug);
		ImGui::InputFloat("Quick Debug Float", &SettingsManager::ms_Dynamic.QuickDebugFloat);

		if (ImGui::TreeNode("Engine##2"))
		{
			bool visualiseDSV = SettingsManager::ms_Dynamic.VisualiseDSVEnabled;
			bool visualiseShadow = SettingsManager::ms_Dynamic.VisualiseShadowMap;

			ImGui::SeparatorText("DX12");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				FixWidthOnNext("Near Plane");
				ImGui::InputFloat("Near Plane", &SettingsManager::ms_Dynamic.NearPlane);
				FixWidthOnNext("Far Plane");
				ImGui::InputFloat("Far Plane", &SettingsManager::ms_Dynamic.FarPlane);

				bool wireframeChanged = ImGui::Checkbox("Wireframe", &SettingsManager::ms_Dynamic.WireframeMode);
				bool cullChanged = ImGui::Checkbox("Backface Culling", &SettingsManager::ms_Dynamic.CullBackFaceEnabled);

				ImGui::Checkbox("Freeze Frustum Culling", &SettingsManager::ms_Dynamic.FreezeFrustum);

				ImGui::Checkbox("Force Show Frustum Debug Lines", &SettingsManager::ms_Dynamic.AlwaysShowFrustumDebugLines);

				ImGui::Checkbox("Show Bounding Spheres", &SettingsManager::ms_Dynamic.BoundingSphereMode);

				ImGui::Checkbox("Transparents Enabled", &SettingsManager::ms_Dynamic.TransparentGOEnabled);
				ImGui::Checkbox("AT GOs Enabled", &SettingsManager::ms_Dynamic.ATGoEnabled);

				float prePassChanged = ImGui::Checkbox("Depth Pre-Pass", &SettingsManager::ms_Dynamic.DepthPrePassEnabled);

				ImGui::Checkbox("Force Sync CPU and GPU", &SettingsManager::ms_Dynamic.ForceSyncCPUGPU);

				ImGui::Checkbox("Shader Optimization", &SettingsManager::ms_Dynamic.ShaderCompilerOptimizationEnabled);

				FixWidthOnNext("Optimization Level");
				ImGui::Text("Optimization Level");

				ImGui::Indent(IM_GUI_INDENTATION);

				static int opt = 3;
				if (ImGui::RadioButton("0", &opt, 0) && opt == 0)
					SettingsManager::ms_Dynamic.ShaderOptimizationLevelFlag = D3DCOMPILE_OPTIMIZATION_LEVEL0;
				ImGui::SameLine();
				if (ImGui::RadioButton("1", &opt, 1) && opt == 1)
					SettingsManager::ms_Dynamic.ShaderOptimizationLevelFlag = D3DCOMPILE_OPTIMIZATION_LEVEL1;
				ImGui::SameLine();
				if (ImGui::RadioButton("2", &opt, 2) && opt == 2)
					SettingsManager::ms_Dynamic.ShaderOptimizationLevelFlag = D3DCOMPILE_OPTIMIZATION_LEVEL2;
				ImGui::SameLine();
				if (ImGui::RadioButton("3", &opt, 3) && opt == 3)
					SettingsManager::ms_Dynamic.ShaderOptimizationLevelFlag = D3DCOMPILE_OPTIMIZATION_LEVEL3;

				ImGui::Unindent(IM_GUI_INDENTATION);

				if (wireframeChanged || cullChanged || prePassChanged || ImGui::Button("Recompile Shaders"))
				{
					d3d->Flush();
					ResourceTracker::RecompileAllShaders(d3d->GetDevice());
				}					
				
				if (visualiseShadow)
					ImGui::BeginDisabled(true);

				ImGui::Checkbox("Visualise Depth Buffer", &SettingsManager::ms_Dynamic.VisualiseDSVEnabled);

				if (visualiseShadow)
					ImGui::EndDisabled();				

				ImGui::BeginDisabled(true);
				if (ImGui::Checkbox("Mip Map Debug Mode (Disabled, Compile-Time only)", &SettingsManager::ms_Dynamic.MipMapDebugMode))
				{
					d3d->Flush();
					ResourceTracker::ClearTexList();
					ResourceTracker::ClearMatList();
					DescriptorManager::Shutdown();
					DescriptorManager::Init(d3d, SettingsManager::ms_DX12.DescriptorHeapSize);
					TextureLoader::Shutdown();
					Scene::StaticShutdown();
					TextureLoader::InitComputeShaders(d3d->GetDevice());
					scene->UnloadContent();
					scene->LoadContent();
				}
				ImGui::EndDisabled();
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Shadow Map");
			ImGui::Indent(IM_GUI_INDENTATION);
			{
				ImGui::Checkbox("Enabled", &SettingsManager::ms_Dynamic.Shadow.Enabled);
				ImGui::Checkbox("Rendering", &SettingsManager::ms_Dynamic.Shadow.Rendering);
				ImGui::Checkbox("Dynamic Bounds", &SettingsManager::ms_Dynamic.Shadow.UpdatingBounds);
				ImGui::Spacing();
				ImGui::Checkbox("Fit to scene and frusta", &SettingsManager::ms_Dynamic.Shadow.BoundToScene);
				ImGui::Checkbox("Use bounding spheres", &SettingsManager::ms_Dynamic.Shadow.UseBoundingSpheres);
				ImGui::Checkbox("Cull Against Bounds", &SettingsManager::ms_Dynamic.Shadow.CullAgainstBounds);
				ImGui::Spacing();

				FixWidthOnNext("Cascade Count");
				ImGui::InputInt("Cascade Count", &SettingsManager::ms_Dynamic.Shadow.CascadeCount);
				FixWidthOnNext("Auto NearFar Percents");
				ImGui::Checkbox("Auto NearFar Percents", &SettingsManager::ms_Dynamic.Shadow.AutoNearFarPercent);
				FixWidthOnNext("Near Percents");
				ImGui::InputFloat4("Near Percents", SettingsManager::ms_Dynamic.Shadow.NearPercents);
				FixWidthOnNext("Far Percents");
				ImGui::InputFloat4("Far Percents", SettingsManager::ms_Dynamic.Shadow.FarPercents);

				if (visualiseDSV || !SettingsManager::ms_Dynamic.Shadow.Enabled)
					ImGui::BeginDisabled(true);

				ImGui::Checkbox("Visualise Map", &SettingsManager::ms_Dynamic.VisualiseShadowMap);

				if (visualiseDSV || !SettingsManager::ms_Dynamic.Shadow.Enabled)
					ImGui::EndDisabled();

				ImGui::Checkbox("Show Bounds", &SettingsManager::ms_Dynamic.Shadow.ShowDebugBounds);
				FixWidthOnNext("Bounds Bias");
				ImGui::InputFloat("Bounds Bias", &SettingsManager::ms_Dynamic.Shadow.BoundsBias);
				FixWidthOnNext("Depth Bias");
				ImGui::InputFloat("Depth Bias", &scene->GetPerFrameCBuffers().ShadowMapPixel.Bias);
				FixWidthOnNext("Normal Depth Bias");
				ImGui::InputFloat("Normal Depth Bias", &scene->GetPerFrameCBuffers().ShadowMap.NormalBias);

				FixWidthOnNext("Frame Wait Count");
				ImGui::InputInt("Frame Wait Count", &SettingsManager::ms_Dynamic.Shadow.TimeSlice);

				FixWidthOnNext("PCF Samples");
				ImGui::Text("PCF Samples");

				ImGui::Indent(IM_GUI_INDENTATION);

				static int pcf = 2;
				if (ImGui::RadioButton("1", &pcf, 0) && pcf == 0)
					SettingsManager::ms_Dynamic.Shadow.PCFSampleCount = 1;
				ImGui::SameLine();
				if (ImGui::RadioButton("4", &pcf, 1) && pcf == 1)
					SettingsManager::ms_Dynamic.Shadow.PCFSampleCount = 4;
				ImGui::SameLine();
				if (ImGui::RadioButton("16", &pcf, 2) && pcf == 2)
					SettingsManager::ms_Dynamic.Shadow.PCFSampleCount = 16;

				ImGui::Unindent(IM_GUI_INDENTATION);
				ImGui::Spacing();
			}			

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Window");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				FixWidthOnNext("Fullscreen");
				if (ImGui::Checkbox("Fullscreen", &SettingsManager::ms_Dynamic.FullscreenEnabled))
					scene->GetWindow()->SetFullscreen(SettingsManager::ms_Dynamic.FullscreenEnabled);

				FixWidthOnNext("VSync");
				ImGui::Checkbox("VSync", &SettingsManager::ms_Dynamic.VSyncEnabled);
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Assets");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				bool prevAllowBinTex = SettingsManager::ms_Dynamic.AllowBinTex;
				ImGui::Checkbox("Force reload all binTex", &SettingsManager::ms_Dynamic.AllowBinTex);
				if (prevAllowBinTex && !SettingsManager::ms_Dynamic.AllowBinTex)
				{
					ResourceTracker::ClearTexList();
					ResourceTracker::ClearMatList();
				}
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("ImGUI");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				ImGui::Checkbox("Show demo window", &SettingsManager::ms_Dynamic.ShowImGuiDemoWindow);
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::TreePop();
			ImGui::Spacing();
		}

		if (ImGui::TreeNode("Scene##2"))
		{
			ImGui::SeparatorText("Visuals");
			ImGui::Indent(IM_GUI_INDENTATION);

			FixWidthOnNext("Background Color");
			ImGui::ColorEdit4("Background Color", reinterpret_cast<float*>(&scene->m_BackgroundColor));

			ImGui::Checkbox("Debug Lines", &SettingsManager::ms_Dynamic.DebugLinesEnabled);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Rendering");
			ImGui::Indent(IM_GUI_INDENTATION);

			FixWidthOnNext("Sort by Depth");
			ImGui::Checkbox("Sort By Depth", &SettingsManager::ms_Dynamic.BatchSortingEnabled);

			FixWidthOnNext("Time Scale");
			ImGui::InputFloat("Time Scale", &SettingsManager::ms_Dynamic.UpdateTimeScale);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Directional Light CBV");
			ImGui::Indent(IM_GUI_INDENTATION);

			FixWidthOnNext("Direction");
			ImGui::InputFloat3("Direction", reinterpret_cast<float*>(&scene->GetPerFrameCBuffers().DirectionalLight.LightDirection));
			scene->GetPerFrameCBuffers().DirectionalLight.LightDirection = Normalize(scene->GetPerFrameCBuffers().DirectionalLight.LightDirection);

			FixWidthOnNext("Light Colour");
			ImGui::ColorEdit4("Light Colour", reinterpret_cast<float*>(&scene->GetPerFrameCBuffers().DirectionalLight.LightDiffuse));
			FixWidthOnNext("Ambient Colour");
			ImGui::ColorEdit3("Ambient Colour", reinterpret_cast<float*>(&scene->GetPerFrameCBuffers().DirectionalLight.AmbientColor));

			FixWidthOnNext("Specular Power");
			ImGui::InputFloat("Specular Power", &scene->GetPerFrameCBuffers().DirectionalLight.SpecularPower);

			ImGui::TreePop();
			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	ImGui::Spacing();
}

void AlkaliGUIManager::RenderGUITools(D3DClass* d3d, Scene* scene)
{
	if (ImGui::CollapsingHeader("Tools"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		if (ImGui::TreeNode("Model Preprocessing"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			string fileDir = "C:\\Users\\finnw\\OneDrive\\Documents\\3D objects\\";
			string msg = "Using directory: " + fileDir;
			ImGui::Text(msg.c_str());

			static char fileName[256];
			ImGui::InputText("File name (.obj)", fileName, 256, 0);

			static bool split = true;
			ImGui::Checkbox("Split Model", &split);

			static bool invert = false;
			ImGui::Checkbox("Invert Model", &invert);

			if (ImGui::Button("Import"))
			{
				string fileNameStr(fileName);
				string filePath = fileDir + fileNameStr + ".obj";
				ModelLoaderObj::PreprocessObjFile(filePath, split, invert);
			}

			ImGui::TreePop();
			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::Spacing();
		}

		if (ImGui::TreeNode("Object Creation"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			static int num = 1;
			ImGui::InputInt("Number", &num);

			static XMFLOAT3 range = XMFLOAT3(20, 10, 20);
			ImGui::InputFloat3("Range", reinterpret_cast<float*>(&range));

			if (ImGui::Button("Instantiate Cube(s)"))
				AssetFactory::InstantiateObjects("Cube.model", num, range);

			ImGui::Spacing();

			if (ImGui::Button("Instantiate Madeline(s)"))
				AssetFactory::InstantiateObjects("Madeline.model", num, range);

			ImGui::Spacing();

			if (ImGui::Button("Instantiate Sphere(s)"))
				AssetFactory::InstantiateObjects("Sphere.model", num, range);

			ImGui::Spacing();

			if (ImGui::Button("Instantiate Plane(s)"))
				AssetFactory::InstantiateObjects("Plane.model", num, range);

			ImGui::Spacing();

			if (ImGui::Button("Instantiate TomBox(es)"))
				AssetFactory::InstantiateObjects("TomBoxTriangulated.model", num, range);

			ImGui::Spacing();

			if (ImGui::Button("Clear Resource Tracker"))
			{
				ResourceTracker::ReleaseAll();
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::Spacing();
			ImGui::TreePop();
		}

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	ImGui::Spacing();
}

void AlkaliGUIManager::RenderTextureGUI(Texture* tex)
{
	ImGui::Indent(IM_GUI_INDENTATION);

	ImGui::Text(("FilePath: " + tex->GetFilePath()).c_str());
	ImGui::Text(("Mip Levels: " + std::to_string(tex->GetMipLevels())).c_str());
	ImGui::Text(("Channels: " + std::to_string(tex->GetChannels())).c_str());
	if (tex->GetHasAlpha())
		ImGui::Text("Transparent: TRUE");

	ImGui::Unindent(IM_GUI_INDENTATION);
}

void AlkaliGUIManager::RenderModelGUI(Model* model)
{
	ImGui::Indent(IM_GUI_INDENTATION);

	string vCountStr = "Model Vertex Count: " + std::to_string(model->GetVertexCount());
	string iCountStr = "Model Index Count: " + std::to_string(model->GetIndexCount());

	string radiStr = "Model Bounding Radius: " + std::to_string(model->GetSphereRadius());
	string centroidStr = "Model Centroid: " + ToString(model->GetCentroid());

	ImGui::Text(("FilePath: " + model->GetFilePath()).c_str());
	ImGui::Text(vCountStr.c_str());
	ImGui::Text(iCountStr.c_str());
	ImGui::Text(radiStr.c_str());
	ImGui::Text(centroidStr.c_str());

	ImGui::Unindent(IM_GUI_INDENTATION);
}

void AlkaliGUIManager::RenderShaderGUI(Shader* shader)
{
	ImGui::Indent(IM_GUI_INDENTATION);

	wstring vs, ps, hs, ds;
	vs = L"VS: " + shader->m_VSName;
	ps = L"PS: " + shader->m_PSName;
	hs = L"HS: " + shader->m_HSName;
	ds = L"DS: " + shader->m_DSName;
	ImGui::Text(wstringToString(vs).c_str());
	ImGui::Text(wstringToString(ps).c_str());
	ImGui::Text(wstringToString(hs).c_str());
	ImGui::Text(wstringToString(ds).c_str());

	ImGui::Unindent(IM_GUI_INDENTATION);
}

void AlkaliGUIManager::RenderMatGUI(Material* mat)
{
	ImGui::Indent(IM_GUI_INDENTATION);

	UINT srv;
	UINT cbvFrame[BACK_BUFFER_COUNT];
	UINT cbvDraw[BACK_BUFFER_COUNT];
	for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		mat->GetIndices(srv, cbvFrame[i], cbvDraw[i], i);	

	if (cbvFrame[0] != -1)
	{
		string indicesFrame = "";
		string indicesDraw = "";
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			indicesFrame += std::to_string(cbvFrame[i]) + "~";
			indicesDraw += std::to_string(cbvDraw[i]) + "~";
		}			

		if (cbvDraw[0] != -1)
			ImGui::Text(("CBV Index (PerDraw): " + indicesDraw).c_str());
		if (cbvFrame[0] != -1)
			ImGui::Text(("CBV Index (PerFrame): " + indicesFrame).c_str());		
	}

	if (srv != -1)
		ImGui::Text(("SRV Index: " + std::to_string(srv)).c_str());

	ImGui::Indent(IM_GUI_INDENTATION);
	{
		auto& matTexList = mat->GetTextures();
		for (size_t t = 0; t < matTexList.size(); t++)
		{
			ImGui::Text(("SRV " + std::to_string(t) + ": " + matTexList[t]->GetFilePath()).c_str());
		}
	}	
	ImGui::Unindent(IM_GUI_INDENTATION);

	MaterialPropertiesCB matProp;
	if (mat->GetProperties(matProp))
	{
		ImGui::Text("Material Properties:");
		bool changed = false;

		ImGui::Indent(IM_GUI_INDENTATION);

		FixWidthOnNext("Base Color");
		changed |= ImGui::ColorEdit3("Base Color", reinterpret_cast<float*>(&matProp.BaseColorFactor));
		FixWidthOnNext("Roughness");
		changed |= ImGui::SliderFloat("Roughness", reinterpret_cast<float*>(&matProp.Roughness), 0, 1);
		FixWidthOnNext("Metalness");
		changed |= ImGui::SliderFloat("Metalness", reinterpret_cast<float*>(&matProp.Metallic), 0, 1);
		FixWidthOnNext("Alpha Cutoff");
		changed |= ImGui::SliderFloat("Alpha Cutoff", reinterpret_cast<float*>(&matProp.AlphaCutoff), 0, 1);
		FixWidthOnNext("Dispersion");
		changed |= ImGui::InputFloat("Dispersion", reinterpret_cast<float*>(&matProp.Dispersion));
		FixWidthOnNext("IOR");
		changed |= ImGui::InputFloat("IOR", reinterpret_cast<float*>(&matProp.IOR));

		ImGui::Unindent(IM_GUI_INDENTATION);

		if (changed)
		{
			mat->SetCBV_PerDraw(1, &matProp, sizeof(MaterialPropertiesCB));
			mat->AttachProperties(matProp);
		}
	}

	ThinFilmCB thinFilm;
	if (mat->GetThinFilm(thinFilm))
	{
		ImGui::Text("Thin Film Interference:");
		bool changed = false;

		ImGui::Indent(IM_GUI_INDENTATION);

		FixWidthOnNext("Thickness Max");
		changed |= ImGui::SliderFloat("Thickness Max", &thinFilm.ThicknessMax, 0, 3000);
		FixWidthOnNext("Thickness Min");
		changed |= ImGui::SliderFloat("Thickness Min", &thinFilm.ThicknessMin, 0, 3000);
		FixWidthOnNext("n0 IOR (External)");
		changed |= ImGui::SliderFloat("n0 IOR (External)", &thinFilm.n0, 0.2f, 3);
		FixWidthOnNext("n1 IOR (Film)");
		changed |= ImGui::SliderFloat("n1 IOR (Film)", &thinFilm.n1, 0.2f, 3);
		FixWidthOnNext("n2 IOR (Internal)");
		changed |= ImGui::SliderFloat("n2 IOR (Internal)", &thinFilm.n2, 0.2f, 3);

		ImGui::Unindent(IM_GUI_INDENTATION);

		if (changed)
		{
			thinFilm.CalculateDelta();
			mat->SetCBV_PerDraw(2, &thinFilm, sizeof(ThinFilmCB));
			mat->AttachThinFilm(thinFilm);
		}

		ImGui::Text(("Delta (readonly): " + std::to_string(thinFilm.GetDelta())).c_str());
	}

	ImGui::Unindent(IM_GUI_INDENTATION);
}

void AlkaliGUIManager::RenderGameObjectGUI(GameObject* go, int id)
{
	ImGui::Indent(IM_GUI_INDENTATION);

	Transform t = go->GetTransform();

	string iStr = "##" + go->m_Name + std::to_string(id);

	float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
	ImGui::InputFloat3(("Position" + iStr).c_str(), pPos);
	float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
	ImGui::InputFloat3(("Rotation" + iStr).c_str(), pRot);
	float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
	ImGui::InputFloat3(("Scale" + iStr).c_str(), pScale);

	t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
	t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
	t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

	go->SetTransform(t);

	ImGui::Checkbox(("Enabled" + iStr).c_str(), go->GetEnabledPtr());

	if (ImGui::TreeNode(("Model Info" + iStr).c_str()))
	{
		RenderModelGUI(go->GetModel());
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(("Shader Info" + iStr).c_str()))
	{
		RenderShaderGUI(go->GetShader());
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(("Texture Info" + iStr).c_str()))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		auto& textures = go->GetMaterial()->GetTextures();
		for (int i = 0; i < textures.size(); i++)
		{
			if (ImGui::CollapsingHeader(textures[i]->GetFilePath().c_str()))
			{
				RenderTextureGUI(textures[i].get());
			}
		}

		ImGui::TreePop();
		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	if (ImGui::TreeNode(("Material Info" + iStr).c_str()))
	{
		RenderMatGUI(go->GetMaterial());
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Unindent(IM_GUI_INDENTATION);
}

void AlkaliGUIManager::RenderGUICurrentScene(D3DClass* d3d, Scene* scene)
{
	if (ImGui::CollapsingHeader("Current Scene"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::SeparatorText("Camera Controls");
			ImGui::Indent(IM_GUI_INDENTATION);

			bool usingFP = scene->GetCamera()->GetMode() == CameraMode::CAMERA_MODE_FP;
			if (!usingFP)
				ImGui::BeginDisabled(true);

			if (ImGui::Button("Scroll Mode"))
			{
				scene->GetCamera()->SetMode(CameraMode::CAMERA_MODE_SCROLL);
			}

			if (!usingFP)
				ImGui::EndDisabled();

			if (usingFP)
				ImGui::BeginDisabled(true);

			if (ImGui::Button("WASD Mode"))
			{
				scene->GetCamera()->SetMode(CameraMode::CAMERA_MODE_FP);
			}

			if (usingFP)
				ImGui::EndDisabled();

			ImGui::InputFloat("Camera Speed", &scene->GetCamera()->GetSpeedLVal());
			ImGui::InputFloat("Camera Rotation Speed", &scene->GetCamera()->GetRotSpeedLVal());

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);

			Transform camTrans = scene->GetCamera()->GetTransform();
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

			scene->GetCamera()->SetTransform(camTrans);

			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		size_t goCount = 0;
		auto& batchList = ResourceTracker::GetBatches();
		for (auto& it : batchList)
		{
			goCount += it.second->GetOpaques().size();
			goCount += it.second->GetTrans().size();
		}

		if (ImGui::Button("Enable All"))
		{
			for (auto& it : batchList)
			{
				for (auto& it2 : it.second->GetOpaques())
					it2.SetEnabled(true);

				for (auto& it2 : it.second->GetTrans())
					it2.SetEnabled(true);

				for (auto& it2 : it.second->GetATs())
					it2.SetEnabled(true);
			}
		}

		if (ImGui::Button("Disable All"))
		{
			for (auto& it : batchList)
			{
				for (auto& it2 : it.second->GetOpaques())
					it2.SetEnabled(false);

				for (auto& it2 : it.second->GetTrans())
					it2.SetEnabled(false);

				for (auto& it2 : it.second->GetATs())
					it2.SetEnabled(false);
			}
		}

		string goTag = "GameObjects (" + std::to_string(goCount) + ")";
		if (ImGui::CollapsingHeader(goTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : batchList)
			{
				auto& opaques = it.second->GetOpaques();
				for (int i = 0; i < opaques.size(); i++)
				{
					if (ImGui::CollapsingHeader(opaques[i].m_Name.c_str()))
					{
						RenderGameObjectGUI(&opaques[i], i);
					}
				}

				auto& atGOs = it.second->GetATs();
				for (int i = 0; i < atGOs.size(); i++)
				{
					if (ImGui::CollapsingHeader(atGOs[i].m_Name.c_str()))
					{
						RenderGameObjectGUI(&atGOs[i], i);
					}
				}

				auto& transGos = it.second->GetTrans();
				for (int i = 0; i < transGos.size(); i++)
				{
					if (ImGui::CollapsingHeader(transGos[i].m_Name.c_str()))
					{
						RenderGameObjectGUI(&transGos[i], i);
					}
				}
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::Spacing();
		}

		string batchTag = "Batches (" + std::to_string(batchList.size()) + ")";
		if (ImGui::CollapsingHeader(batchTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : batchList)
			{
				if (ImGui::CollapsingHeader(it.second->m_Name.c_str()))
				{
					ImGui::Indent(IM_GUI_INDENTATION);

					auto& opaques = it.second->GetOpaques();
					string tag = "Opaques (" + std::to_string(opaques.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < opaques.size(); i++)
						{
							if (ImGui::CollapsingHeader(opaques[i].m_Name.c_str()))
							{
								RenderGameObjectGUI(&opaques[i], i);
							}
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					auto& ats = it.second->GetATs();
					tag = "Alpha Tested (" + std::to_string(ats.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < ats.size(); i++)
						{
							if (ImGui::CollapsingHeader(ats[i].m_Name.c_str()))
							{
								RenderGameObjectGUI(&ats[i], i);
							}
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					auto& trans = it.second->GetTrans();
					tag = "Transparents (" + std::to_string(trans.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < trans.size(); i++)
						{
							if (ImGui::CollapsingHeader(trans[i].m_Name.c_str()))
							{
								RenderGameObjectGUI(&trans[i], i);
							}
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					ImGui::Spacing();
					ImGui::Unindent(IM_GUI_INDENTATION);
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& texList = ResourceTracker::GetTextures();
		string texTag = "Textures (" + std::to_string(texList.size()) + ")";
		if (ImGui::CollapsingHeader(texTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : texList)
			{
				if (ImGui::CollapsingHeader(it.first.c_str()))
				{
					RenderTextureGUI(it.second.get());
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& modelList = ResourceTracker::GetModels();
		string modelTag = "Models (" + std::to_string(modelList.size()) + ")";
		if (ImGui::CollapsingHeader(modelTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : modelList)
			{
				if (ImGui::CollapsingHeader(it.first.c_str()))
				{
					RenderModelGUI(it.second.get());
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& shaderList = ResourceTracker::GetShaders();
		string shaderTag = "Shaders (" + std::to_string(shaderList.size()) + ")";
		if (ImGui::CollapsingHeader(shaderTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : shaderList)
			{
				if (ImGui::CollapsingHeader(it.first.c_str()))
				{
					RenderShaderGUI(it.second.get());
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		auto& matList = ResourceTracker::GetMaterials();
		string matTag = "Materials (" + std::to_string(matList.size()) + ")";
		if (ImGui::CollapsingHeader(matTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (size_t i = 0; i < matList.size(); i++)
			{
				if (ImGui::CollapsingHeader(("Material #" + std::to_string(i)).c_str()))
				{
					RenderMatGUI(matList[i].get());
				}
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		if (!SettingsManager::ms_Dynamic.HeapDebugViewEnabled)
			ImGui::BeginDisabled(true);

		if (ImGui::CollapsingHeader("Heap View"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			auto list = DescriptorManager::GetDebugHeapStrings();

			for (size_t i = 0; i < list.size(); i++)
			{
				ImGui::Text((std::to_string(i) + ". " + list[i]).c_str());
			}

			ImGui::Spacing();
			ImGui::Unindent(IM_GUI_INDENTATION);
		}

		if (!SettingsManager::ms_Dynamic.HeapDebugViewEnabled)
			ImGui::EndDisabled();

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	ImGui::Spacing();
}

void AlkaliGUIManager::RenderGUISceneList(D3DClass* d3d, Scene* currentScene, Application* app)
{
	if (ImGui::CollapsingHeader("Scene List"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		for (const auto& pair : app->GetSceneMap())
		{
			bool disabled = pair.second.get() == currentScene;
			if (disabled)
				ImGui::BeginDisabled(true);

			if (ImGui::Button(wstringToString(pair.first).c_str()))
			{
				app->AssignScene(pair.second.get());
			}

			if (disabled)
				ImGui::EndDisabled();
		}

		ImGui::Spacing();

		if (ImGui::Button("Reset current scene"))
		{
			app->AssignScene(currentScene);
		}

		ImGui::Unindent(IM_GUI_INDENTATION);
	}

	ImGui::Spacing();
}
