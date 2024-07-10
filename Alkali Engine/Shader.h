#pragma once

#include "pch.h"
#include "Application.h"

using std::wstring_view;
using std::wstring;
using std::vector;

const wstring g_dirPath = L"Assets/Shaders/";

struct ShaderArgs
{
	wstring vs, ps;
	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	ID3D12RootSignature* rootSig;

	bool cullNone = false;
	bool cullFrontOnly = false;
	bool disableDSV = false;
	bool disableDSVWrite = false;
	bool NoPS = false;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	DXGI_FORMAT RTVFormat = SettingsManager::ms_DX12.RTVFormat;

	ShaderArgs& operator=(const ShaderArgs& other)
	{
		if (this == &other)
			return *this;

		vs = other.vs;
		ps = other.ps;
		inputLayout = other.inputLayout; 
		rootSig = other.rootSig;

		cullNone = other.cullNone;
		cullFrontOnly = other.cullFrontOnly;
		disableDSV = other.disableDSV;
		disableDSVWrite = other.disableDSVWrite;
		Topology = other.Topology;
		RTVFormat = other.RTVFormat;
		NoPS = other.NoPS;

		return *this;
	}
};

class Shader
{
public:
	void Init(ID3D12Device2* device, const ShaderArgs& args);
	void InitPreCompiled(ID3D12Device2* device, const ShaderArgs& args);

	void Compile(ID3D12Device2* device);
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
	ComPtr<ID3D12PipelineState> m_pso = nullptr;

	bool m_preCompiled = false;
	ShaderArgs m_args;
};

