#pragma once

#include "Scene.h"

class Tutorial2 : public Scene
{
public:
	using super = Scene;

	Tutorial2(const std::wstring& name, int width, int height, bool vSync = false);

	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

protected:
	virtual void OnUpdate(UpdateEventArgs& e) override;

	virtual void OnRender(RenderEventArgs& e) override;

	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;

private:
	float m_FoV;

	XMMATRIX m_ViewMatrix;
	XMMATRIX m_ProjectionMatrix;

	bool m_ContentLoaded = false;

	shared_ptr<Model> m_modelCube;
	shared_ptr<Model> m_modelMadeline;
	shared_ptr<Batch> m_batch;
	shared_ptr<GameObject> m_goCube;
	shared_ptr<Shader> m_shaderCube;	
};