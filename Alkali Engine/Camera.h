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
	CameraMode GetMode();
	void SetSpeed(float speed);

	void Update(TimeEventArgs& e);	

	void Reset();

	XMMATRIX GetViewMatrix();	

private:
	void MoveFirstPerson(TimeEventArgs& e);
	void MoveScroll(TimeEventArgs& e);

	CameraMode m_currentMode;
	float m_speed = 3;
	float m_rotationSpeed = 2;

	XMFLOAT3 m_upVector, m_forwardVector, m_rightVector;
	float m_pitch, m_yaw;
};

