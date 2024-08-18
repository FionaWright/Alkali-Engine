#include "pch.h"
#include "SceneTrueEmpty.h"
#include "ImGUIManager.h"
#include "ModelLoaderObj.h"
#include "ModelLoaderGLTF.h"
#include "ResourceTracker.h"
#include "Utils.h"
#include "TextureLoader.h"
#include "RootSig.h"
#include "AssetFactory.h"

SceneTrueEmpty::SceneTrueEmpty(const std::wstring& name, Window* pWindow)
	: Scene(name, pWindow, true)
{
}

bool SceneTrueEmpty::LoadContent()
{	
	return true;
}

void SceneTrueEmpty::UnloadContent()
{
	Scene::UnloadContent();
}

void SceneTrueEmpty::OnUpdate(TimeEventArgs& e)
{
	Scene::OnUpdate(e);
}

void SceneTrueEmpty::OnRender(TimeEventArgs& e)
{
	Scene::OnRender(e);
}