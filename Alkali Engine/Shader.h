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

	void Init(const wstring& vsName, const wstring& psName, D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT inputLayoutCount, ComPtr<ID3D12RootSignature> rootSig, ComPtr<ID3D12Device2> device);
	void InitPreCompiled(const wstring& vsName, const wstring& psName, D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT inputLayoutCount, ComPtr<ID3D12RootSignature> rootSig, ComPtr<ID3D12Device2> device, wstring exePath);

	void Compile(ComPtr<ID3D12Device2> device);

	ComPtr<ID3D12PipelineState> GetPSO();

protected:
	ComPtr<ID3DBlob> CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target);

	ComPtr<ID3DBlob> ReadPreCompiledShader(LPCWSTR path);

private:
	wstring m_vsName, m_psName, m_hsName, m_dsName;
	D3D12_INPUT_ELEMENT_DESC* m_inputLayout = nullptr;
	UINT m_inputLayoutCount = 0;

	ComPtr<ID3D12RootSignature> m_rootSig;
	ComPtr<ID3D12PipelineState> m_pso;

	bool m_preCompiled = false;
};

