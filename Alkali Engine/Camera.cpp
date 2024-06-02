#include "pch.h"
#include "Camera.h"
#include "InputManager.h"

Camera::Camera(CameraMode mode)
	: GameObject("Camera", nullptr, nullptr)
	, m_currentMode(mode)
{
}

Camera::~Camera()
{
}

void Camera::SetMode(CameraMode mode)
{
	m_currentMode = mode;
}

void Camera::SetSpeed(float speed)
{
	m_speed = speed;
}

void Camera::Update(UpdateEventArgs& eUpdate)
{
	if (m_currentMode == CameraMode::CAMERA_MODE_FP)
		MoveFirstPerson(eUpdate);
}

void Camera::MoveFirstPerson(UpdateEventArgs& eUpdate)
{
	if (InputManager::IsKeyDown(KeyCode::R))
	{
		SetPosition(0, 0, -10);
		SetRotation(0, 0, 0);
		return;
	}

	float speed = m_speed * static_cast<float>(eUpdate.ElapsedTime);
	float rotSpeed = m_rotationSpeed * static_cast<float>(eUpdate.ElapsedTime);

	float sinX = sin(m_transform.Rotation.x);
	float cosX = cos(m_transform.Rotation.x);
	float sinY = sin(m_transform.Rotation.y);
	float cosY = cos(m_transform.Rotation.y);
	float sinZ = sin(m_transform.Rotation.z);
	float cosZ = cos(m_transform.Rotation.z);

	if (InputManager::IsKey(KeyCode::E))
		m_transform.Position.y += speed;

	if (InputManager::IsKey(KeyCode::Q))
		m_transform.Position.y -= speed;

	if (InputManager::IsKey(KeyCode::X))
		m_transform.Rotation.y += rotSpeed;

	if (InputManager::IsKey(KeyCode::Z))
		m_transform.Rotation.y -= rotSpeed;

	m_transform.Rotation.y = static_cast<float>(fmod(m_transform.Rotation.y, 360));

	if (InputManager::IsKey(KeyCode::C))
	{
		if (InputManager::IsKey(KeyCode::A))
			m_transform.Rotation.z += rotSpeed;

		if (InputManager::IsKey(KeyCode::D))
			m_transform.Rotation.z -= rotSpeed;

		if (InputManager::IsKey(KeyCode::W))
			m_transform.Rotation.x += rotSpeed;

		if (InputManager::IsKey(KeyCode::S))
			m_transform.Rotation.x -= rotSpeed;

		m_transform.Rotation.x = static_cast<float>(fmod(m_transform.Rotation.x, 360));
		m_transform.Rotation.z = static_cast<float>(fmod(m_transform.Rotation.z, 360));

		return;
	}

	if (InputManager::IsKey(KeyCode::A))
	{
		m_transform.Position.x -= cosY * cosZ * speed;
		m_transform.Position.y -= sinZ * speed;
		m_transform.Position.z += sinY * cosZ * speed;
	}

	if (InputManager::IsKey(KeyCode::D))
	{
		m_transform.Position.x += cosY * cosZ * speed;
		m_transform.Position.y += sinZ * speed;
		m_transform.Position.z -= sinY * cosZ * speed;
	}

	if (InputManager::IsKey(KeyCode::W))
	{
		m_transform.Position.x += sinY * speed;
		m_transform.Position.z += cosY * speed;
	}

	if (InputManager::IsKey(KeyCode::S))
	{
		m_transform.Position.x -= sinY * speed;
		m_transform.Position.z -= cosY * speed;
	}
}

XMMATRIX Camera::GetViewMatrix()
{	
	XMFLOAT3 up = XMFLOAT3(0, 1, 0);
	XMVECTOR upVector = XMLoadFloat3(&up);

	XMFLOAT3 lookAt = XMFLOAT3(0, 0, 1);
	XMVECTOR lookAtVector = XMLoadFloat3(&lookAt);

	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_transform.Rotation.x, m_transform.Rotation.y, m_transform.Rotation.z);

	lookAtVector = XMVector3TransformCoord(lookAtVector, R);
	upVector = XMVector3TransformCoord(upVector, R);

	XMVECTOR positionVector = XMLoadFloat3(&m_transform.Position);
	lookAtVector = XMVectorAdd(positionVector, lookAtVector);

	return XMMatrixLookAtLH(positionVector, lookAtVector, upVector);
}
