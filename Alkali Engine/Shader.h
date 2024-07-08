#pragma once

#include "pch.h"
#include "Application.h"

using std::wstring_view;
using std::wstring;
using std::vector;

const wstring g_dirPath = L"Assets/Shaders/";

class Shader
{
public:
	Shader();
	~Shader();

	void Init(const wstring& vsName, const wstring& psName, vector<D3D12_INPUT_ELEMENT_DESC> inputLayout, ID3D12RootSignature* rootSig, ID3D12Device2* device, bool cullNone = false, bool disableDSV = false, bool disableDSVWrite = false, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	void InitPreCompiled(const wstring& vsName, const wstring& psName, vector<D3D12_INPUT_ELEMENT_DESC> inputLayout, ID3D12RootSignature* rootSig, ID3D12Device2* device, bool cullNone = false, bool disableDSV = false, bool disableDSVWrite = false, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	void Compile(ID3D12Device2* device, ID3D12RootSignature* rootSig);
	void Recompile(ID3D12Device2* device);

	ComPtr<ID3D12PipelineState> GetPSO();

	bool IsPreCompiled();

	wstring m_VSName, m_PSName, m_HSName, m_DSName;

	static bool ms_GlobalFillWireframeMode;
	static bool ms_GlobalCullNone;

protected:
	ComPtr<ID3DBlob> CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target);

	ComPtr<ID3DBlob> ReadPreCompiledShader(LPCWSTR path);

private:
	vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE m_topology;

	ID3D12RootSignature* m_rootSig = nullptr;
	ComPtr<ID3D12PipelineState> m_pso = nullptr;

	bool m_preCompiled = false;
	bool m_cullNone = false;
	bool m_disableDSV = false, m_disableDSVWrite = false;
};

