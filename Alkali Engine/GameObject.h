#pragma once

#include "pch.h"
#include "Model.h"
#include "Shader.h"

using std::shared_ptr;

class Model;

class GameObject
{
public:
	struct Transform 
	{
		XMFLOAT3 Position;
		XMFLOAT3 Rotation;
		XMFLOAT3 Scale;
	};

	GameObject();
	~GameObject();

	void Init(shared_ptr<Model> pModel, shared_ptr<Shader> pShader, ComPtr<ID3D12PipelineState> pPSO);
	void Render(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12RootSignature> rootSig, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, XMMATRIX viewProj);

	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xyz);
	void SetRotation(float x, float y, float z);
	void SetRotation(XMFLOAT3 xyz);
	void SetScale(float x, float y, float z);
	void SetScale(XMFLOAT3 xyz);

	void UpdateWorldMatrix();

	Transform m_transform;

private:
	ComPtr<ID3D12PipelineState> m_pso;
	shared_ptr<Model> m_model;
	shared_ptr<Shader> m_shader;

	XMMATRIX m_worldMatrix;
};

