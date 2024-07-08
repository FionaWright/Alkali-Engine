#pragma once

#include "Scene.h"

class SceneBistro : public Scene
{
public:
	SceneBistro(const std::wstring& name, Window* pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;

private:
	GameObject* m_goSkybox = nullptr;
};