#include "pch.h"
#include "GameObject.h"
#include "Constants.h"
#include "SettingsManager.h"
#include "Utils.h"
#include "Scene.h"
#include "Batch.h"
#include "AssetFactory.h"

GameObject::GameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial, bool orthographic)
	: m_transform({})
	, m_worldMatrix(XMMatrixIdentity())
	, m_model(pModel)
	, m_shader(pShader)
	, m_Name(name)
	, m_material(pMaterial)
	, m_orthographic(orthographic)
	, m_isTransparent(false)
{	
}

GameObject::GameObject(string name)
	: m_transform({})
	, m_worldMatrix(XMMatrixIdentity())
	, m_model(nullptr)
	, m_shader(nullptr)
	, m_Name(name)
	, m_material(nullptr)
{
}

GameObject::~GameObject()
{
}

void GameObject::Render(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, const RootParamInfo& rpi, const int& backBufferIndex, bool* requireCPUGPUSync, MatricesCB* matrices, RenderOverride* renderOverride, Shader** lastSetShader)
{
	if (!m_model || (!m_shader && !renderOverride->ShaderOverride))
		throw std::exception("Missing components");

	if (!m_model->IsLoaded())
		return;

	if (!m_enabled || (renderOverride && renderOverride->UseDepthMaterial && !m_isOccluder))
		return;	

	if (!m_shader->IsInitialised())
		return;

	bool matLoaded = m_material->IsLoaded();
	if (!matLoaded)
	{
		m_material->TryUploadSRVs(d3d);
		return;
	}		

	bool materialCorrectState = m_material->EnsureCorrectSRVState(cmdListDirect);
	if (requireCPUGPUSync)
		*requireCPUGPUSync |= !materialCorrectState;

	if (renderOverride && renderOverride->UseDepthMaterial)
	{
		if (m_shadowMapMats.size() <= renderOverride->DepthMatIndex)
			m_shadowMapMats.resize(renderOverride->DepthMatIndex + 1);

		if (!m_shadowMapMats.at(renderOverride->DepthMatIndex))
		{
			auto mat = AssetFactory::CreateMaterial();
			vector<UINT> sizes = { sizeof(MatricesCB) };
			mat->AddCBVs(d3d, cmdListDirect, sizes, false);

			if (SettingsManager::ms_DX12.DepthAlphaTestEnabled && renderOverride->AddSRVToDepthMat && m_material->GetTextures().size() > 0)
			{
				mat->AddSRVs(d3d, { m_material->GetTextures().at(0) });
			}

			m_shadowMapMats[renderOverride->DepthMatIndex] = mat;
		}
		if (!materialCorrectState)
			return;
	}

	Shader* usedShader = m_shader.get();
	if (renderOverride && renderOverride->ShaderOverride)
	{
		if (!renderOverride->ShaderOverride->IsInitialised())
			return;

		usedShader = renderOverride->ShaderOverride;
	}		

	if (!lastSetShader || usedShader != *lastSetShader)
	{
		cmdListDirect->SetPipelineState(usedShader->GetPSO().Get());

		if (lastSetShader)
			*lastSetShader = usedShader;
	}		

	if (m_orthographic)
	{
		m_material->AssignMaterial(cmdListDirect, rpi, backBufferIndex);
		m_model->Render(cmdListDirect);
		return;
	}

	if (!matrices)
		throw std::exception("Matrices not given");

	Material* matUsed = m_material.get();
	if (renderOverride && renderOverride->UseDepthMaterial)
		matUsed = m_shadowMapMats[renderOverride->DepthMatIndex].get();

	Model* usedModel = m_model.get();
	if (renderOverride && renderOverride->ModelOverride)
		usedModel = renderOverride->ModelOverride;

	if (renderOverride && renderOverride->ModifyTransformToCentroid)
	{		
		Transform modifiedTransform = m_transform;
		XMFLOAT3 centroidScaled = Mult(m_transform.Scale, m_model->GetCentroid());
		float maxScale = std::max(std::max(m_transform.Scale.x, m_transform.Scale.y), m_transform.Scale.z);
		modifiedTransform.Scale = Mult(XMFLOAT3_ONE, m_model->GetSphereRadius() * maxScale);
		modifiedTransform.Position = Add(m_transform.Position, centroidScaled);

		RenderModel(cmdListDirect, rpi, backBufferIndex, matrices, usedModel, &modifiedTransform, matUsed);
		return;
	}

	RenderModel(cmdListDirect, rpi, backBufferIndex, matrices, usedModel, nullptr, matUsed);
}

void GameObject::RenderModel(ID3D12GraphicsCommandList2* cmdListDirect, const RootParamInfo& rpi, const int& backBufferIndex, MatricesCB* matrices, Model* model, Transform* transform, Material* material)
{
	if (transform)
		matrices->M = TransformToWorldMatrix(*transform);
	else
		matrices->M = m_worldMatrix;
	matrices->InverseTransposeM = XMMatrixTranspose(XMMatrixInverse(nullptr, matrices->M));
	material->SetCBV_PerDraw(0, matrices, sizeof(MatricesCB), backBufferIndex);

	material->AssignMaterial(cmdListDirect, rpi, backBufferIndex);

	model->Render(cmdListDirect);
}

Transform GameObject::GetTransform() const
{
	return m_transform;
}

void GameObject::SetTransform(const Transform& t)
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

void GameObject::SetOccluderState(bool enabled)
{
	m_isOccluder = enabled;
}

void GameObject::SetEnabled(bool enabled)
{
	m_enabled = enabled;
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
	if (SettingsManager::ms_Misc.CentroidBasedWorldMatricesEnabled && considerCentroid && m_model)
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

void GameObject::ForceSetTransparent(bool trans)
{
	m_isTransparent = trans;
}

void GameObject::ForceSetAT(bool at)
{
	m_isAT = at;
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

Model* GameObject::GetModel() const
{
	return m_model.get();
}

Shader* GameObject::GetShader() const
{
	return m_shader.get();
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

bool* GameObject::GetEnabledPtr()
{
	return &m_enabled;
}

bool GameObject::IsTransparent() const
{
	return m_isTransparent;
}

bool GameObject::IsAT() const
{
	return m_material->GetHasAlpha() || m_isAT;
}

bool GameObject::IsOrthographic() const
{
	return m_orthographic;
}
