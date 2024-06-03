#include "pch.h"
#include "Camera.h"
#include "InputManager.h"

Camera::Camera(CameraMode mode)
	: GameObject("Camera", nullptr, nullptr)
	, m_currentMode(mode)
	, m_upVector(XMFLOAT3(0, 1, 0))
	, m_forwardVector(XMFLOAT3(0, 0, 1))
	, m_rightVector(XMFLOAT3(1, 0, 0))
{	
}

Camera::~Camera()
{
}

void Camera::SetMode(CameraMode mode)
{
	m_currentMode = mode;
}

CameraMode Camera::GetMode()
{
	return m_currentMode;
}

void Camera::SetSpeed(float speed)
{
	m_speed = speed;
}

void Camera::Update(TimeEventArgs& e)
{
	if (m_currentMode == CameraMode::CAMERA_MODE_FP)
		MoveFirstPerson(e);
	else if (m_currentMode == CameraMode::CAMERA_MODE_SCROLL)
		MoveScroll(e);
}

void Camera::Reset()
{
	m_pitch = 0;
	m_yaw = 0;
	SetRotation(0, 0, 0);
	m_upVector = XMFLOAT3(0, 1, 0);
	m_forwardVector = XMFLOAT3(0, 0, 1);
	m_rightVector = XMFLOAT3(1,0,0);
}

void Camera::MoveFirstPerson(TimeEventArgs& e)
{
	if (InputManager::IsKeyDown(KeyCode::R))
	{
		SetPosition(0, 0, -10);
		SetRotation(0, 0, 0);
		return;
	}

	float speed = m_speed * static_cast<float>(e.ElapsedTime);
	float rotSpeed = m_rotationSpeed * static_cast<float>(e.ElapsedTime);

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

void Camera::MoveScroll(TimeEventArgs& e)
{
	if (InputManager::IsMouseRight())
	{
		XMFLOAT2 deltaMouse = InputManager::GetMousePosDelta();
		float rotationSpeed = 1.5f * e.ElapsedTime * m_rotationSpeed;
		deltaMouse.x *= rotationSpeed;
		deltaMouse.y *= rotationSpeed;

		m_pitch += deltaMouse.y;
		m_yaw += deltaMouse.x;
		XMVECTOR rot = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
		rot = XMQuaternionNormalize(rot);
		XMStoreFloat4(&m_transform.Rotation, rot);

		XMVECTOR direction = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rot);
		XMStoreFloat3(&m_forwardVector, direction);

		XMVECTOR up = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rot);
		XMStoreFloat3(&m_upVector, up);

		XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rot);
		XMStoreFloat3(&m_rightVector, right);
		return;
	}

	if (InputManager::IsMouseMiddle())
	{
		XMFLOAT2 deltaMouse = InputManager::GetMousePosDelta();
		float panSpeed = -3.5f * e.ElapsedTime * m_speed;
		deltaMouse.x *= panSpeed;
		deltaMouse.y *= panSpeed;

		XMFLOAT3 upTranslation = XMFLOAT3(m_upVector.x * -deltaMouse.y, m_upVector.y * -deltaMouse.y, m_upVector.z * -deltaMouse.y);
		AddPosition(upTranslation);

		XMFLOAT3 rightTranslation = XMFLOAT3(m_rightVector.x * deltaMouse.x, m_rightVector.y * deltaMouse.x, m_rightVector.z * deltaMouse.x);
		AddPosition(rightTranslation);
		return;
	}

	float mouseWheelDelta = InputManager::GetMouseWheelDelta();
	mouseWheelDelta *= 230 * e.ElapsedTime * m_speed;
	XMFLOAT3 forwardTranslation = XMFLOAT3(m_forwardVector.x * mouseWheelDelta, m_forwardVector.y * mouseWheelDelta, m_forwardVector.z * mouseWheelDelta);
	AddPosition(forwardTranslation);
}

XMMATRIX Camera::GetViewMatrix()
{	
	if (m_currentMode == CameraMode::CAMERA_MODE_FP)
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

	XMVECTOR up = XMLoadFloat3(&m_upVector);
	XMVECTOR dir = XMLoadFloat3(&m_forwardVector);

	XMVECTOR positionVector = XMLoadFloat3(&m_transform.Position);

	return XMMatrixLookToLH(positionVector, dir, up);
}
