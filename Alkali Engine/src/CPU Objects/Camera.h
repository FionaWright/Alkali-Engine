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

	void SetRotation(float x, float y, float z) override;

	void Update(TimeEventArgs& e);	

	void Reset();

	XMMATRIX GetViewMatrix();	

	float& GetSpeedLVal();
	float& GetRotSpeedLVal();

private:
	void MoveFirstPerson(TimeEventArgs& e);
	void MoveScroll(TimeEventArgs& e);

	CameraMode m_currentMode;
	float m_speed = 0.0225f;
	float m_rotationSpeed = 0.015f;

	XMFLOAT3 m_upVector, m_forwardVector, m_rightVector;
	float m_pitch = 0, m_yaw = 0;
};

