#pragma once

#include "Scene.h"

class CubeScene : public Scene
{
public:
	CubeScene(const std::wstring& name, Window* pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;

private:
	float m_FoV;

	shared_ptr<Model> m_modelCube;
	shared_ptr<Batch> m_batch;
	shared_ptr<GameObject> m_goCube;
	shared_ptr<Shader> m_shaderCube;
	shared_ptr<Material> m_material;
	shared_ptr<Texture> m_texture, m_normalMap;
};