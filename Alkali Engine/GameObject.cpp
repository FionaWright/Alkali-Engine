#include "pch.h"
#include "GameObject.h"
#include "Settings.h"

GameObject::GameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader)
	: m_transform({})
	, m_worldMatrix(XMMatrixIdentity())
	, m_model(pModel)
	, m_shader(pShader)
	, m_Name(name)
{
	SetPosition(0, 0, 0);
	SetRotation(0, 0, 0);
	SetScale(1, 1, 1);
}

GameObject::~GameObject()
{
}

void GameObject::Render(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12RootSignature> rootSig, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj)
{
	if (!m_model || !m_shader)
		return;

	commandList->SetPipelineState(m_shader->GetPSO().Get());
	commandList->SetGraphicsRootSignature(rootSig.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->RSSetViewports(1, &viewPort);
	commandList->RSSetScissorRects(1, &scissorRect);

	UINT numRenderTargets = 1;
	commandList->OMSetRenderTargets(numRenderTargets, &rtv, FALSE, &dsv);

	XMMATRIX mvpMatrix = XMMatrixMultiply(m_worldMatrix, viewProj);
	commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);

	m_model->Render(commandList);
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

	m_transform.Rotation = XMFLOAT3(x, y, z);
	m_transform.Rotation.x *= degToRad;
	m_transform.Rotation.y *= degToRad;
	m_transform.Rotation.z *= degToRad;
	UpdateWorldMatrix();
}

void GameObject::SetRotation(XMFLOAT3 xyz)
{
	float degToRad = static_cast<float>(DEG_TO_RAD);

	m_transform.Rotation = xyz;
	m_transform.Rotation.x *= degToRad;
	m_transform.Rotation.y *= degToRad;
	m_transform.Rotation.z *= degToRad;
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
