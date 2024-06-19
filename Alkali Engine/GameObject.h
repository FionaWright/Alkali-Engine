#pragma once

#include "pch.h"
#include "Model.h"
#include "Shader.h"
#include "Material.h"

using std::shared_ptr;

class Model;
class Shader;

struct Transform
{
	XMFLOAT3 Position;
	XMFLOAT3 Rotation;
	XMFLOAT3 Scale;
};

class GameObject
{
public:
	GameObject(string name, shared_ptr<Model> pModel, shared_ptr<Shader> pShader, shared_ptr<Material> pMaterial = nullptr);
	~GameObject();

	void Render(ID3D12GraphicsCommandList2* commandListDirect, ID3D12RootSignature* rootSig, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj);

	Transform GetTransform();
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

	size_t GetModelVertexCount();
	size_t GetModelIndexCount();
	void GetShaderNames(wstring& vs, wstring& ps, wstring& hs, wstring& ds);
	void GetBoundingSphere(XMFLOAT3& position, float& radius);

	bool IsTransparent();

	string m_Name;

protected:	
	shared_ptr<Model> m_model;
	shared_ptr<Shader> m_shader;
	shared_ptr<Material> m_material;

	Transform m_transform;

	XMMATRIX m_worldMatrix;
};

