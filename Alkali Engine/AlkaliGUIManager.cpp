#include "pch.h"
#include "AlkaliGUIManager.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"
#include "DescriptorManager.h"
#include "InputManager.h"
#include "ModelLoader.h"
#include "Camera.h"
#include "Scene.h"
#include "Application.h"
#include "AssetFactory.h"

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

	RenderGUISettings(d3d, scene);
	RenderGUITools(d3d, scene);
	RenderGUISceneList(d3d, scene, app);
	RenderGUICurrentScene(d3d, scene);

	if (SettingsManager::ms_Dynamic.ShowImGuiDemoWindow)
		ImGui::ShowDemoWindow();
}

void AlkaliGUIManager::RenderGUISettings(D3DClass* d3d, Scene* scene)
{
	auto& shaderList = ResourceTracker::GetShaders();

	if (ImGui::CollapsingHeader("Settings"))
	{
		ImGui::Indent(IM_GUI_INDENTATION);

		ImGui::Checkbox("Quick Debug", &SettingsManager::ms_Dynamic.QuickDebug);

		if (ImGui::TreeNode("Engine##2"))
		{
			ImGui::SeparatorText("DX12");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				ImGui::InputFloat("Near Plane", &SettingsManager::ms_Dynamic.NearPlane);
				ImGui::InputFloat("Far Plane", &SettingsManager::ms_Dynamic.FarPlane);

				bool wireframeChanged = ImGui::Checkbox("Wireframe", &SettingsManager::ms_Dynamic.WireframeMode);
				bool cullChanged = ImGui::Checkbox("Backface Culling", &SettingsManager::ms_Dynamic.CullFaceEnabled);

				if (wireframeChanged || cullChanged)
				{
					for (auto& it : shaderList)
					{
						it.second->Recompile(d3d->GetDevice());
					}
				}

				ImGui::Checkbox("Freeze Frustum Culling", &SettingsManager::ms_Dynamic.FreezeFrustum);

				ImGui::Checkbox("Force Show Frustum Debug Lines", &SettingsManager::ms_Dynamic.AlwaysShowFrustumDebugLines);

				ImGui::Checkbox("Show Bounding Spheres", &SettingsManager::ms_Dynamic.BoundingSphereMode);

				ImGui::Checkbox("Force Sync CPU and GPU", &SettingsManager::ms_Dynamic.ForceSyncCPUGPU);

				bool visualiseDSV = SettingsManager::ms_Dynamic.VisualiseDSVEnabled;
				bool visualiseShadow = SettingsManager::ms_Dynamic.VisualiseShadowMap;
				if (visualiseShadow)
					ImGui::BeginDisabled(true);

				ImGui::Checkbox("Visualise Depth Buffer", &SettingsManager::ms_Dynamic.VisualiseDSVEnabled);

				if (visualiseShadow)
					ImGui::EndDisabled();

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
					ImGui::InputInt("Cascade Count", &SettingsManager::ms_Dynamic.Shadow.CascadeCount);
					ImGui::Checkbox("Auto NearFar Percents", &SettingsManager::ms_Dynamic.Shadow.AutoNearFarPercent);
					ImGui::InputFloat4("Near Percents", SettingsManager::ms_Dynamic.Shadow.NearPercents);
					ImGui::InputFloat4("Far Percents", SettingsManager::ms_Dynamic.Shadow.FarPercents);					

					if (visualiseDSV || !SettingsManager::ms_Dynamic.Shadow.Enabled)
						ImGui::BeginDisabled(true);

					ImGui::Checkbox("Visualise Map", &SettingsManager::ms_Dynamic.VisualiseShadowMap);

					if (visualiseDSV || !SettingsManager::ms_Dynamic.Shadow.Enabled)
						ImGui::EndDisabled();

					ImGui::Checkbox("Show Bounds", &SettingsManager::ms_Dynamic.Shadow.ShowDebugBounds);
					ImGui::InputFloat("Bounds Bias", &SettingsManager::ms_Dynamic.Shadow.BoundsBias);
					ImGui::InputFloat("Depth Bias", &scene->GetPerFrameCBuffers().ShadowMapPixel.Bias);
					ImGui::InputFloat("Normal Depth Bias", &scene->GetPerFrameCBuffers().ShadowMap.NormalBias);

					ImGui::InputInt("Frame Wait Count", &SettingsManager::ms_Dynamic.Shadow.TimeSlice);

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

				if (ImGui::Checkbox("Mip Map Debug Mode", &SettingsManager::ms_Dynamic.MipMapDebugMode))
				{
					ResourceTracker::ClearTexList();
					ResourceTracker::ClearMatList();
					DescriptorManager::Shutdown();
					DescriptorManager::Init(d3d, SettingsManager::ms_DX12.DescriptorHeapSize);
					TextureLoader::Shutdown();
					Scene::StaticShutdown();
					scene->UnloadContent();
					scene->LoadContent();
				}
			}

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Window");
			ImGui::Indent(IM_GUI_INDENTATION);

			{
				if (ImGui::Checkbox("Fullscreen", &SettingsManager::ms_Dynamic.FullscreenEnabled))
					scene->GetWindow()->SetFullscreen(SettingsManager::ms_Dynamic.FullscreenEnabled);

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

			ImGui::ColorEdit4("Background Color", reinterpret_cast<float*>(&scene->m_BackgroundColor));

			ImGui::Checkbox("Debug Lines", &SettingsManager::ms_Dynamic.DebugLinesEnabled);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Rendering");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::Checkbox("Sort By Depth", &SettingsManager::ms_Dynamic.BatchSortingEnabled);

			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::SeparatorText("Directional Light CBV");
			ImGui::Indent(IM_GUI_INDENTATION);

			ImGui::InputFloat3("Direction", reinterpret_cast<float*>(&scene->GetPerFrameCBuffers().DirectionalLight.LightDirection));
			scene->GetPerFrameCBuffers().DirectionalLight.LightDirection = Normalize(scene->GetPerFrameCBuffers().DirectionalLight.LightDirection);

			ImGui::ColorEdit4("Light Colour", reinterpret_cast<float*>(&scene->GetPerFrameCBuffers().DirectionalLight.LightDiffuse));
			ImGui::ColorEdit3("Ambient Colour", reinterpret_cast<float*>(&scene->GetPerFrameCBuffers().DirectionalLight.AmbientColor));

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
				ModelLoader::PreprocessObjFile(filePath, split, invert);
			}

			ImGui::TreePop();
			ImGui::Unindent(IM_GUI_INDENTATION);
			ImGui::Spacing();
		}

		if (ImGui::TreeNode("Object Creation"))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			static char numObjectsStr[256];
			ImGui::InputText("Number", numObjectsStr, 256, 0);

			static XMFLOAT3 range = XMFLOAT3(20, 10, 20);
			ImGui::InputFloat3("Range", reinterpret_cast<float*>(&range));

			int num = std::atoi(numObjectsStr);

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
				{
					it2.SetEnabled(true);
				}

				for (auto& it2 : it.second->GetTrans())
				{
					it2.SetEnabled(true);
				}
			}
		}

		if (ImGui::Button("Disable All"))
		{
			for (auto& it : batchList)
			{
				for (auto& it2 : it.second->GetOpaques())
				{
					it2.SetEnabled(false);
				}

				for (auto& it2 : it.second->GetTrans())
				{
					it2.SetEnabled(false);
				}
			}
		}

		string goTag = "GameObjects (" + std::to_string(goCount) + ")";
		if (ImGui::CollapsingHeader(goTag.c_str()))
		{
			ImGui::Indent(IM_GUI_INDENTATION);

			for (auto& it : batchList)
			{
				auto& opaques = it.second->GetOpaques();
				for (size_t i = 0; i < opaques.size(); i++)
				{
					if (ImGui::CollapsingHeader(opaques[i].m_Name.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						Transform t = opaques[i].GetTransform();

						string iStr = "##" + opaques[i].m_Name + std::to_string(i);

						float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
						ImGui::InputFloat3(("Position" + iStr).c_str(), pPos);
						float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
						ImGui::InputFloat3(("Rotation" + iStr).c_str(), pRot);
						float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
						ImGui::InputFloat3(("Scale" + iStr).c_str(), pScale);

						t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
						t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
						t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

						opaques[i].SetTransform(t);

						ImGui::Checkbox(("Enabled" + iStr).c_str(), opaques[i].GetEnabledPtr());

						if (ImGui::TreeNode(("Model Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							string vCountStr = "Model Vertex Count: " + std::to_string(opaques[i].GetModelVertexCount());
							string iCountStr = "Model Index Count: " + std::to_string(opaques[i].GetModelIndexCount());

							XMFLOAT3 pos;
							float radius;
							opaques[i].GetBoundingSphere(pos, radius);
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
							opaques[i].GetShaderNames(vs, ps, hs, ds);
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

						if (ImGui::TreeNode(("Texture Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							auto& textures = opaques[i].GetMaterial()->GetTextures();
							for (int i = 0; i < textures.size(); i++)
							{
								ImGui::Text((std::to_string(i) + ". " + textures[i]->GetFilePath()).c_str());

								ImGui::Indent(IM_GUI_INDENTATION);
								ImGui::Text(("Mip Levels: " + std::to_string(textures[i]->GetMipLevels())).c_str());
								ImGui::Unindent(IM_GUI_INDENTATION);
							}

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}
				}

				auto& transGos = it.second->GetTrans();
				for (size_t i = 0; i < transGos.size(); i++)
				{
					if (ImGui::CollapsingHeader(transGos[i].m_Name.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						Transform t = transGos[i].GetTransform();

						string iStr = "##" + transGos[i].m_Name + std::to_string(i);

						float pPos[3] = { t.Position.x, t.Position.y, t.Position.z };
						ImGui::InputFloat3(("Position" + iStr).c_str(), pPos);
						float pRot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
						ImGui::InputFloat3(("Rotation" + iStr).c_str(), pRot);
						float pScale[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
						ImGui::InputFloat3(("Scale" + iStr).c_str(), pScale);

						t.Position = XMFLOAT3(pPos[0], pPos[1], pPos[2]);
						t.Rotation = XMFLOAT3(pRot[0], pRot[1], pRot[2]);
						t.Scale = XMFLOAT3(pScale[0], pScale[1], pScale[2]);

						transGos[i].SetTransform(t);

						ImGui::Checkbox(("Enabled" + iStr).c_str(), transGos[i].GetEnabledPtr());

						if (ImGui::TreeNode(("Model Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							string vCountStr = "Model Vertex Count: " + std::to_string(transGos[i].GetModelVertexCount());
							string iCountStr = "Model Index Count: " + std::to_string(transGos[i].GetModelIndexCount());

							XMFLOAT3 pos;
							float radius;
							transGos[i].GetBoundingSphere(pos, radius);
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
							transGos[i].GetShaderNames(vs, ps, hs, ds);
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

						if (ImGui::TreeNode(("Texture Info" + iStr).c_str()))
						{
							ImGui::Indent(IM_GUI_INDENTATION);

							auto& textures = transGos[i].GetMaterial()->GetTextures();
							for (int i = 0; i < textures.size(); i++)
							{
								ImGui::Text((std::to_string(i) + ". " + textures[i]->GetFilePath()).c_str());

								ImGui::Indent(IM_GUI_INDENTATION);
								ImGui::Text(("Mip Levels: " + std::to_string(textures[i]->GetMipLevels())).c_str());
								ImGui::Unindent(IM_GUI_INDENTATION);
							}

							ImGui::TreePop();
							ImGui::Unindent(IM_GUI_INDENTATION);
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
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

					auto opaques = it.second->GetOpaques();
					string tag = "Opaques (" + std::to_string(opaques.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < opaques.size(); i++)
						{
							ImGui::Text(opaques[i].m_Name.c_str());
						}

						ImGui::Spacing();
						ImGui::Unindent(IM_GUI_INDENTATION);
					}

					auto trans = it.second->GetTrans();
					tag = "Transparents (" + std::to_string(trans.size()) + ")##" + it.first;
					if (ImGui::CollapsingHeader(tag.c_str()))
					{
						ImGui::Indent(IM_GUI_INDENTATION);

						for (int i = 0; i < trans.size(); i++)
						{
							ImGui::Text(trans[i].m_Name.c_str());
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
				ImGui::Text(it.first.c_str());

				ImGui::Indent(IM_GUI_INDENTATION);
				ImGui::Text(("Mip Levels: " + std::to_string(it.second->GetMipLevels())).c_str());
				ImGui::Text(("Channels: " + std::to_string(it.second->GetChannels())).c_str());
				if (it.second->GetHasAlpha())
					ImGui::Text("Transparent: TRUE");

				ImGui::Unindent(IM_GUI_INDENTATION);
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
				ImGui::Text(it.first.c_str());
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
				ImGui::Text(it.first.c_str());

				if (it.second->IsPreCompiled())
				{
					ImGui::Indent(IM_GUI_INDENTATION);

					ImGui::Text("Precompiled: TRUE");

					ImGui::Unindent(IM_GUI_INDENTATION);
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
				UINT srv, cbvDraw;
				UINT cbvFrame[BACK_BUFFER_COUNT];
				for (int i = 0; i < BACK_BUFFER_COUNT; i++)
					matList[i]->GetIndices(srv, cbvFrame[i], cbvDraw, i);

				ImGui::Text(("Material: " + std::to_string(i)).c_str());

				ImGui::Indent(IM_GUI_INDENTATION);

				if (cbvFrame[0] != -1)
				{
					string indices = "";
					for (int i = 0; i < BACK_BUFFER_COUNT; i++)
						indices += std::to_string(cbvFrame[i]) + "~";
					ImGui::Text(("CBV Index (PerFrame): " + indices).c_str());
				}					
				if (cbvDraw != -1)
					ImGui::Text(("CBV Index (PerDraw): " + std::to_string(cbvDraw)).c_str());
				if (srv != -1)
					ImGui::Text(("SRV Index: " + std::to_string(srv)).c_str());

				ImGui::Indent(IM_GUI_INDENTATION);

				auto& matTexList = matList[i]->GetTextures();
				for (size_t t = 0; t < matTexList.size(); t++)
				{
					ImGui::Text(("SRV " + std::to_string(t) + ": " + matTexList[t]->GetFilePath()).c_str());
				}

				ImGui::Unindent(IM_GUI_INDENTATION);

				MaterialPropertiesCB matProp;
				if (matList[i]->GetProperties(matProp))
				{
					ImGui::Text("Material Properties:");

					ImGui::Indent(IM_GUI_INDENTATION);

					ImGui::Text(("BaseColor: " + ToString(matProp.BaseColorFactor)).c_str());
					ImGui::Text(("Roughness: " + std::to_string(matProp.Roughness)).c_str());
					ImGui::Text(("Metalness: " + std::to_string(matProp.Metallic)).c_str());
					ImGui::Text(("Alpha Cutoff: " + std::to_string(matProp.AlphaCutoff)).c_str());
					ImGui::Text(("Dispersion: " + std::to_string(matProp.Dispersion)).c_str());
					ImGui::Text(("IOR: " + std::to_string(matProp.IOR)).c_str());

					ImGui::Unindent(IM_GUI_INDENTATION);
				}

				ImGui::Unindent(IM_GUI_INDENTATION);
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
