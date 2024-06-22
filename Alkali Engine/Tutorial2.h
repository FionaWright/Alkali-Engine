#pragma once

#include "Scene.h"

class Tutorial2 : public Scene
{
public:
	Tutorial2(const std::wstring& name, Window* pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(TimeEventArgs& e) override;

	virtual void OnRender(TimeEventArgs& e) override;

private:
	shared_ptr<Model> m_modelTest;
	shared_ptr<Batch> m_batch;
	GameObject* m_goTest;
	shared_ptr<Shader> m_shaderPBR, m_shaderPBR_CullOff;	
	shared_ptr<Material> m_material;
	shared_ptr<Texture> m_texture, m_normalMap;	
};