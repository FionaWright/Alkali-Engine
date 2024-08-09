#pragma once

#include "pch.h"
#include "SettingsManager.h"
#include "ImGUIManager.h"
#include "Constants.h"
#include "D3DClass.h"

using std::string;
using std::vector;

class Scene;
class Application;

class AlkaliGUIManager
{
public:
	static void RenderGUI(D3DClass* d3d, Scene* scene, Application* app);
	static void LogErrorMessage(string msg);

private:
	static void RenderGUISettings(D3DClass* d3d, Scene* scene);
	static void RenderGUITools(D3DClass* d3d, Scene* scene);
	static void RenderGUICurrentScene(D3DClass* d3d, Scene* scene);
	static void RenderGUISceneList(D3DClass* d3d, Scene* scene, Application* app);

	static vector<string> ms_errorList;
};

