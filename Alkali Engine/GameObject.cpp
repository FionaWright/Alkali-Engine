#include "pch.h"
#include "GameObject.h"
#include "Settings.h"
#include "CBuffers.h"

GameObject::GameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial)
	: m_transform({})
	, m_worldMatrix(XMMatrixIdentity())
	, m_model(pModel)
	, m_shader(pShader)
	, m_Name(name)
	, m_material(pMaterial)
{
	SetPosition(0, 0, 0);
	SetRotation(0, 0, 0);
	SetScale(1, 1, 1);
}

GameObject::~GameObject()
{
}

void GameObject::Render(ComPtr<ID3D12GraphicsCommandList2> commandListDirect, ComPtr<ID3D12RootSignature> rootSig, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj)
{
	if (!m_model || !m_shader)
		return;

	commandListDirect->SetPipelineState(m_shader->GetPSO().Get());
	commandListDirect->SetGraphicsRootSignature(rootSig.Get());
	commandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandListDirect->RSSetViewports(1, &viewPort);
	commandListDirect->RSSetScissorRects(1, &scissorRect);

	UINT numRenderTargets = 1;
	commandListDirect->OMSetRenderTargets(numRenderTargets, &rtv, FALSE, &dsv);

	if (m_material)
	{
		auto samplerHeap = m_material->GetSamplerHeap();
		ID3D12DescriptorHeap* ppHeaps[] = { samplerHeap.Get() };
		commandListDirect->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		commandListDirect->SetGraphicsRootDescriptorTable(1, samplerHeap->GetGPUDescriptorHandleForHeapStart());
	}

	MatricesCB matricesCB;
	matricesCB.M = m_worldMatrix;
	matricesCB.InverseTransposeM = XMMatrixInverse(nullptr, XMMatrixTranspose(m_worldMatrix));
	matricesCB.VP = viewProj;
	commandListDirect->SetGraphicsRoot32BitConstants(0, sizeof(MatricesCB) / 4, &matricesCB, 0);

	m_model->Render(commandListDirect);
}

Transform GameObject::GetTransform()
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

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(x, y, z);
	XMStoreFloat4(&m_transform.Rotation, quaternion);
	UpdateWorldMatrix();
}

void GameObject::SetRotation(XMFLOAT3 xyz)
{
	SetRotation(xyz.x, xyz.y, xyz.z);
}

void GameObject::SetRotation(XMFLOAT4 quat)
{
	XMVECTOR quaternion = XMLoadFloat4(&quat);
	XMStoreFloat4(&m_transform.Rotation, quaternion);
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

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(x, y, z);
	XMVECTOR curRot = XMLoadFloat4(&m_transform.Rotation);
	curRot = XMQuaternionMultiply(curRot, quaternion);
	curRot = XMQuaternionNormalize(curRot);

	XMStoreFloat4(&m_transform.Rotation, curRot);
	UpdateWorldMatrix();
}

void GameObject::RotateBy(XMFLOAT3 xyz)
{
	RotateBy(xyz.x, xyz.y, xyz.z);
}

void GameObject::RotateBy(XMFLOAT4 quat)
{
	XMVECTOR quaternion = XMLoadFloat4(&quat);
	XMVECTOR curRot = XMLoadFloat4(&m_transform.Rotation);
	curRot = XMQuaternionMultiply(curRot, quaternion);
	curRot = XMQuaternionNormalize(curRot);

	XMStoreFloat4(&m_transform.Rotation, curRot);
	UpdateWorldMatrix();
}

void GameObject::UpdateWorldMatrix()
{
	XMMATRIX T = XMMatrixTranslation(m_transform.Position.x, m_transform.Position.y, m_transform.Position.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_transform.Rotation.x, m_transform.Rotation.y, m_transform.Rotation.z);
	XMMATRIX S = XMMatrixScaling(m_transform.Scale.x, m_transform.Scale.y, m_transform.Scale.z);

	m_worldMatrix = XMMatrixMultiply(S, XMMatrixMultiply(R, T));
}

size_t GameObject::GetModelVertexCount()
{
	return m_model->GetVertexCount();
}

size_t GameObject::GetModelIndexCount()
{
	return m_model->GetIndexCount();
}

void GameObject::GetShaderNames(wstring& vs, wstring& ps, wstring& hs, wstring& ds)
{
	vs = m_shader->m_VSName;
	ps = m_shader->m_PSName;
	hs = m_shader->m_HSName;
	ds = m_shader->m_DSName;
}
