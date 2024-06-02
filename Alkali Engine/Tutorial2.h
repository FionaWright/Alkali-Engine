#pragma once

#include "Scene.h"

class Tutorial2 : public Scene
{
public:
	using super = Scene;

	Tutorial2(const std::wstring& name, shared_ptr<Window> pWindow);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(UpdateEventArgs& e) override;

	virtual void OnRender(RenderEventArgs& e) override;

private:
	float m_FoV;

	XMMATRIX m_ViewMatrix;
	XMMATRIX m_ProjectionMatrix;

	shared_ptr<Model> m_modelCube;
	shared_ptr<Model> m_modelMadeline;
	shared_ptr<Batch> m_batch;
	shared_ptr<GameObject> m_goCube;
	shared_ptr<Shader> m_shaderCube;	
};