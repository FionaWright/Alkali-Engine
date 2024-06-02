#pragma once

#include "pch.h"
#include "GameObject.h"

enum CameraMode
{
	CAMERA_MODE_FP,
	CAMERA_MODE_SCROLL
};

class Camera : public GameObject
{
public:
	Camera(CameraMode mode);
	~Camera();

	void SetMode(CameraMode mode);
	void SetSpeed(float speed);

	void Update(TimeEventArgs& eUpdate);	

	XMMATRIX GetViewMatrix();	

private:
	void MoveFirstPerson(TimeEventArgs& eUpdate);

	CameraMode m_currentMode;
	float m_speed = 3;
	float m_rotationSpeed = 2;
};

