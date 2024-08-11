#pragma once

#include "Scene.h"

class SceneChess : public Scene
{
public:
	SceneChess(const std::wstring& name, Window* pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;

private:
	GameObject* m_goSkybox = nullptr;
};