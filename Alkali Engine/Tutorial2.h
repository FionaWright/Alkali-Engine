#pragma once

#include "Scene.h"

class Tutorial2 : public Scene
{
public:
	Tutorial2(const std::wstring& name, shared_ptr<Window> pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;

private:
	shared_ptr<Model> m_modelMadeline;
	shared_ptr<Batch> m_batch;
	shared_ptr<GameObject> m_goCube;
	shared_ptr<Shader> m_shaderCube;	
	shared_ptr<Material> m_material;
	shared_ptr<Texture> m_texture, m_normalMap;	
};