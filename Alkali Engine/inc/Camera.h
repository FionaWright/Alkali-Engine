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

	void Update(UpdateEventArgs& eUpdate);	

	XMMATRIX GetViewMatrix();	

private:
	void MoveFirstPerson(UpdateEventArgs& eUpdate);

	CameraMode m_currentMode;
	float m_speed = 3;
	float m_rotationSpeed = 2;
};

