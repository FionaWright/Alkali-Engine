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
	float m_FoV;

	shared_ptr<Batch> m_batch;
	shared_ptr<Shader> m_shaderPBR, m_shaderPBR_CullOff;
	GameObject* m_goSkybox;
};