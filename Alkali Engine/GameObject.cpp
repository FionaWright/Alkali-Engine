#include "pch.h"
#include "GameObject.h"
#include "Settings.h"
#include "Utils.h"
#include "Scene.h"

GameObject::GameObject(string name, RootParamInfo& rpi, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial, bool orthographic)
	: m_transform({})
	, m_worldMatrix(XMMatrixIdentity())
	, m_model(pModel)
	, m_shader(pShader)
	, m_Name(name)
	, m_material(pMaterial)
	, m_orthographic(orthographic)
	, m_rootParamInfo(rpi)
{
}

GameObject::GameObject(string name)
	: m_transform({})
	, m_worldMatrix(XMMatrixIdentity())
	, m_model(nullptr)
	, m_shader(nullptr)
	, m_Name(name)
	, m_material(nullptr)
	, m_orthographic(false)
{
}

GameObject::~GameObject()
{
}

void GameObject::Render(ID3D12GraphicsCommandList2* commandListDirect, PerFrameCBuffers* perFrameCB, MatricesCB* matrices)
{
	if (!m_model || !m_shader)
		throw std::exception("Missing components");

	commandListDirect->SetPipelineState(m_shader->GetPSO().Get());	

	if (m_orthographic)
	{
		m_model->Render(commandListDirect);
		return;
	}

	if (!matrices)
		throw std::exception("Matrices not given");

	Model* sphereModel = nullptr;	
	if (Scene::IsSphereModeOn(&sphereModel))
	{		
		Transform modifiedTransform = m_transform;
		XMFLOAT3 centroidScaled = Mult(m_transform.Scale, m_model->GetCentroid());
		float maxScale = std::max(std::max(m_transform.Scale.x, m_transform.Scale.y), m_transform.Scale.z);
		modifiedTransform.Scale = Mult(XMFLOAT3_ONE, m_model->GetSphereRadius() * maxScale);
		modifiedTransform.Position = Add(m_transform.Position, centroidScaled);

		RenderModel(commandListDirect, perFrameCB, matrices, sphereModel, &modifiedTransform);
		return;
	}

	RenderModel(commandListDirect, perFrameCB, matrices, m_model.get());
}

void GameObject::RenderModel(ID3D12GraphicsCommandList2* commandListDirect, PerFrameCBuffers* perFrameCB, MatricesCB* matrices, Model* model, Transform* transform)
{
	XMMATRIX& worldMatrix = m_worldMatrix;
	if (transform)
		worldMatrix = TransformToWorldMatrix(*transform);

	matrices->M = worldMatrix;
	matrices->InverseTransposeM = XMMatrixTranspose(XMMatrixInverse(nullptr, worldMatrix));
	m_material->SetCBV_PerDraw(0, matrices, sizeof(MatricesCB));

	if (perFrameCB)
	{
		m_material->SetCBV_PerFrame(0, &perFrameCB->Camera, sizeof(CameraCB));
		m_material->SetCBV_PerFrame(1, &perFrameCB->DirectionalLight, sizeof(DirectionalLightCB));
	}

	m_material->AssignMaterial(commandListDirect, m_rootParamInfo);

	model->Render(commandListDirect);
}

Transform GameObject::GetTransform() const
{
	return m_transform;
}

void GameObject::SetTransform(Transform t)
{
	m_transform = t;
	UpdateWorldMatrix();
}

void GameObject::SetPosition(float x, float y, float z)
{
	m_transform.Position = XMFLOAT3(x, y, z);
	UpdateWorldMatrix();
}

void GameObject::SetPosition(XMFLOAT3 xyz)
{
	m_transform.Position = xyz;
	UpdateWorldMatrix();
}

void GameObject::SetRotation(float x, float y, float z)
{
	float degToRad = static_cast<float>(DEG_TO_RAD);
	x *= degToRad;
	y *= degToRad;
	z *= degToRad;

	m_transform.Rotation = XMFLOAT3(x, y, z);
	UpdateWorldMatrix();
}

void GameObject::SetRotationRadians(float x, float y, float z)
{
	m_transform.Rotation = XMFLOAT3(x, y, z);
	UpdateWorldMatrix();
}

void GameObject::SetRotation(XMFLOAT3 xyz)
{
	SetRotation(xyz.x, xyz.y, xyz.z);
}

void GameObject::SetScale(float x) 
{
	m_transform.Scale = XMFLOAT3(x, x, x);
	UpdateWorldMatrix();
}

void GameObject::SetScale(float x, float y, float z)
{
	m_transform.Scale = XMFLOAT3(x, y, z);
	UpdateWorldMatrix();
}

void GameObject::SetScale(XMFLOAT3 xyz)
{
	m_transform.Scale = xyz;
	UpdateWorldMatrix();
}

void GameObject::AddPosition(float x, float y, float z)
{
	m_transform.Position.x += x;
	m_transform.Position.y += y;
	m_transform.Position.z += z;
	UpdateWorldMatrix();
}

void GameObject::AddPosition(XMFLOAT3 xyz)
{
	AddPosition(xyz.x, xyz.y, xyz.z);
}

void GameObject::RotateBy(float x, float y, float z)
{
	float degToRad = static_cast<float>(DEG_TO_RAD);
	x *= degToRad;
	y *= degToRad;
	z *= degToRad;

	m_transform.Rotation = Add(m_transform.Rotation, XMFLOAT3(x, y, z));
	UpdateWorldMatrix();
}

void GameObject::RotateBy(XMFLOAT3 xyz)
{
	RotateBy(xyz.x, xyz.y, xyz.z);
}

void GameObject::UpdateWorldMatrix(bool considerCentroid)
{
	XMMATRIX C = XMMatrixIdentity();
	if (CENTROID_BASED_WORLD_MATRIX_ENABLED && considerCentroid && m_model)
	{
		XMFLOAT3 centroid = m_model->GetCentroid();
		if (!Equals(centroid, XMFLOAT3_ZERO))
			C = XMMatrixTranslation(centroid.x, centroid.y, centroid.z);
	}
	XMMATRIX CI = XMMatrixInverse(nullptr, C);

	XMMATRIX T = XMMatrixTranslation(m_transform.Position.x, m_transform.Position.y, m_transform.Position.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_transform.Rotation.x, m_transform.Rotation.y, m_transform.Rotation.z);
	XMMATRIX S = XMMatrixScaling(m_transform.Scale.x, m_transform.Scale.y, m_transform.Scale.z);

	m_worldMatrix = XMMatrixMultiply(CI, XMMatrixMultiply(S, XMMatrixMultiply(R, XMMatrixMultiply(T, C))));
}

XMMATRIX GameObject::TransformToWorldMatrix(const Transform& transform) 
{
	XMMATRIX T = XMMatrixTranslation(transform.Position.x, transform.Position.y, transform.Position.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z);
	XMMATRIX S = XMMatrixScaling(transform.Scale.x, transform.Scale.y, transform.Scale.z);

	return XMMatrixMultiply(S, XMMatrixMultiply(R, T));
}

size_t GameObject::GetModelVertexCount() const
{
	return m_model->GetVertexCount();
}

size_t GameObject::GetModelIndexCount() const
{
	return m_model->GetIndexCount();
}

void GameObject::GetShaderNames(wstring& vs, wstring& ps, wstring& hs, wstring& ds) const
{
	vs = m_shader->m_VSName;
	ps = m_shader->m_PSName;
	hs = m_shader->m_HSName;
	ds = m_shader->m_DSName;
}

void GameObject::GetBoundingSphere(XMFLOAT3& position, float& radius) const
{
	radius = m_model->GetSphereRadius() * std::max(std::max(m_transform.Scale.x, m_transform.Scale.y), m_transform.Scale.z);
	position = GetWorldPosition();
}

XMFLOAT3 GameObject::GetWorldPosition() const
{
	if (!m_model)
		return m_transform.Position;

	XMFLOAT3 centroidScaled = Mult(m_transform.Scale, m_model->GetCentroid());
	return Add(centroidScaled, m_transform.Position);
}

Material* GameObject::GetMaterial() const
{
	return m_material.get();
}

bool GameObject::IsTransparent() const
{
	return m_material->GetHasAlpha();
}

bool GameObject::IsOrthographic() const
{
	return m_orthographic;
}
