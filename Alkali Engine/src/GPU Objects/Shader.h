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
	vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;
	ID3D12RootSignature* RootSig = nullptr;

	bool CullNone = false;
	bool CullFront = false;
	bool DisableDSV = false;
	bool DisableDSVWriting = false;
	bool EnableDSVWritingForce = false;
	bool IsDepthShader = false;
	bool DisableStencil = false;
	bool NoPS = false;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	DXGI_FORMAT RTVFormat = SettingsManager::ms_DX12.RTVFormat;
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_D32_FLOAT;

	float SlopeScaleDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	float DepthBias = 0.0f;

	ShaderArgs& operator=(const ShaderArgs& other)
	{
		if (this == &other)
			return *this;

		vs = other.vs;
		ps = other.ps;
		InputLayout = other.InputLayout; 
		RootSig = other.RootSig;

		CullNone = other.CullNone;
		CullFront = other.CullFront;
		DisableDSV = other.DisableDSV;
		DisableDSVWriting = other.DisableDSVWriting;
		Topology = other.Topology;
		RTVFormat = other.RTVFormat;
		NoPS = other.NoPS;
		SlopeScaleDepthBias = other.SlopeScaleDepthBias;
		DSVFormat = other.DSVFormat;
		DisableStencil = other.DisableStencil;
		IsDepthShader = other.IsDepthShader;
		DepthBias = other.DepthBias;
		EnableDSVWritingForce = other.EnableDSVWritingForce;

		return *this;
	}
};

class Shader
{
public:
	void Init(ID3D12Device2* device, const ShaderArgs& args);
	void InitPreCompiled(ID3D12Device2* device, const ShaderArgs& args);

	void Compile(ID3D12Device2* device, bool exitOnFail = false);
	void TryHotReload(ID3D12Device2* device);

	ComPtr<ID3D12PipelineState> GetPSO();

	bool IsPreCompiled() const;
	bool IsInitialised() const;

	wstring m_VSName, m_PSName, m_HSName, m_DSName;

protected:
	ComPtr<ID3DBlob> CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target, bool exitOnFail);

	ComPtr<ID3DBlob> ReadPreCompiledShader(LPCWSTR path);

private:
	ComPtr<ID3D12PipelineState> m_pso = nullptr;

	std::chrono::system_clock::duration m_lastVSTime, m_lastPSTime;

	bool m_preCompiled = false;
	ShaderArgs m_args;
	bool m_initialised = false;
};

