#pragma once

#include "pch.h"
#include "Model.h"
#include "Shader.h"
#include "Material.h"
#include "CBuffers.h"

using std::shared_ptr;

class Model;
class Shader;

struct Transform
{
	XMFLOAT3 Position = XMFLOAT3_ZERO;
	XMFLOAT3 Rotation = XMFLOAT3_ZERO;
	XMFLOAT3 Scale = XMFLOAT3_ONE;
};

struct RootParamInfo
{
	UINT ParamCount;

	UINT NumCBV_PerFrame;
	UINT NumCBV_PerDraw;
	UINT NumSRV;

	UINT ParamIndexCBV_PerFrame;
	UINT ParamIndexCBV_PerDraw;
	UINT ParamIndexSRV;
};

class GameObject
{
public:
	GameObject(string name, RootParamInfo& rpi, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial = nullptr, bool orthoGraphic = false);
	GameObject(string name);
	~GameObject();

	void Render(ID3D12GraphicsCommandList2* commandListDirect, MatricesCB* matrices = nullptr);

	void RenderModel(ID3D12GraphicsCommandList2* commandListDirect, MatricesCB* matrices, Model* model, Transform* transform = nullptr);

	Transform GetTransform() const;
	void SetTransform(Transform t);

	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xyz);
	void SetRotation(float x, float y, float z);
	void SetRotationRadians(float x, float y, float z);
	void SetRotation(XMFLOAT3 xyz);
	void SetScale(float x);
	void SetScale(float x, float y, float z);
	void SetScale(XMFLOAT3 xyz);

	void AddPosition(float x, float y, float z);
	void AddPosition(XMFLOAT3 xyz);

	void RotateBy(float x, float y, float z);
	void RotateBy(XMFLOAT3 xyz);

	void UpdateWorldMatrix(bool considerCentroid = true);	

	size_t GetModelVertexCount() const;
	size_t GetModelIndexCount() const;
	void GetShaderNames(wstring& vs, wstring& ps, wstring& hs, wstring& ds) const;
	void GetBoundingSphere(XMFLOAT3& position, float& radius) const;
	XMFLOAT3 GetWorldPosition() const;
	Material* GetMaterial() const;
	bool IsTransparent() const;
	bool IsOrthographic() const;

	string m_Name;

protected:	
	XMMATRIX TransformToWorldMatrix(const Transform& transform);

	shared_ptr<Model> m_model;
	shared_ptr<Shader> m_shader;
	shared_ptr<Material> m_material;

	Transform m_transform;

	XMMATRIX m_worldMatrix;
	RootParamInfo m_rootParamInfo;

	bool m_orthographic;
};

