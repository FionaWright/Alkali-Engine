#pragma once

#include "pch.h"
#include "SettingsManager.h"
#include "ImGUIManager.h"
#include "Constants.h"
#include "D3DClass.h"

#include <shared_mutex>

using std::string;
using std::vector;

class Scene;
class Application;
class Model;
class Texture;
class Shader;
class Material;
class GameObject;

class AlkaliGUIManager
{
public:
	static void FixWidthOnNext(const char* label);
	static void RenderGUI(D3DClass* d3d, Scene* scene, Application* app);
	static void LogErrorMessage(string msg);
	static void LogAsyncMessage(string msg);

private:
	static void RenderGUISettings(D3DClass* d3d, Scene* scene);
	static void RenderGUITools(D3DClass* d3d, Scene* scene);
	static void RenderTextureGUI(Texture* tex);
	static void RenderModelGUI(Model* model);
	static void RenderShaderGUI(Shader* shader);
	static void RenderMatGUI(Material* mat);
	static void RenderGameObjectGUI(GameObject* go, int id);
	static void RenderGUICurrentScene(D3DClass* d3d, Scene* scene);
	static void RenderGUISceneList(D3DClass* d3d, Scene* scene, Application* app);

	static vector<string> ms_errorLog, ms_asyncLog;
	static std::shared_mutex ms_asyncLogMutex;
};

