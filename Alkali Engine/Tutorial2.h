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
	GameObject* m_goTest, * m_goPlane;
	shared_ptr<Material> m_perFramePBRMat;
};