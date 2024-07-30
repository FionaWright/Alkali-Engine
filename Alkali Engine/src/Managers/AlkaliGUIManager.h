#pragma once

#include "pch.h"
#include "SettingsManager.h"
#include "ImGUIManager.h"
#include "Constants.h"
#include "D3DClass.h"

class Scene;
class Application;

class AlkaliGUIManager
{
public:
	static void RenderGUI(D3DClass* d3d, Scene* scene, Application* app);

private:
	static void RenderGUISettings(D3DClass* d3d, Scene* scene);
	static void RenderGUITools(D3DClass* d3d, Scene* scene);
	static void RenderGUICurrentScene(D3DClass* d3d, Scene* scene);
	static void RenderGUISceneList(D3DClass* d3d, Scene* scene, Application* app);
};

