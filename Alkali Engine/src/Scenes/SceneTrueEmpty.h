#pragma once

#include "Scene.h"

class SceneTrueEmpty : public Scene
{
public:
	SceneTrueEmpty(const std::wstring& name, Window* pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;
};