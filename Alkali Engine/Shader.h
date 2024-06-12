#pragma once

#include "pch.h"
#include "Application.h"

using std::wstring_view;
using std::wstring;

const wstring g_dirPath = L"Assets/Shaders/";

class Shader
{
public:
	Shader();
	~Shader();

	void Init(const wstring& vsName, const wstring& psName, D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT inputLayoutCount, ID3D12RootSignature* rootSig, ID3D12Device2* device, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	void InitPreCompiled(const wstring& vsName, const wstring& psName, D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT inputLayoutCount, ID3D12RootSignature* rootSig, ID3D12Device2* device, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	void Compile(ID3D12Device2* device, ID3D12RootSignature* rootSig);

	ComPtr<ID3D12PipelineState> GetPSO();

	wstring m_VSName, m_PSName, m_HSName, m_DSName;

	static bool ms_FillWireframeMode;
	static bool ms_CullNone;

protected:
	ComPtr<ID3DBlob> CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target);

	ComPtr<ID3DBlob> ReadPreCompiledShader(LPCWSTR path);

private:
	D3D12_INPUT_ELEMENT_DESC* m_inputLayout = nullptr;
	UINT m_inputLayoutCount = 0;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE m_topology;

	ComPtr<ID3D12PipelineState> m_pso;

	bool m_preCompiled = false;
};

