#pragma once

#include "Scene.h"

class SceneTest : public Scene
{
public:
	SceneTest(const std::wstring& name, Window* pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;

private:
	GameObject* m_goTest = nullptr, * m_goPlane = nullptr, * m_goSkybox = nullptr;
};