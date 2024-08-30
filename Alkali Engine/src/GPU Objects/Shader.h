#pragma once

#include "pch.h"
#include "Application.h"

using std::wstring_view;
using std::wstring;
using std::string;
using std::vector;

const wstring g_dirPath = L"Assets/Shaders/";

struct ShaderArgs
{
	wstring VS = L"", PS = L"";
	vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;
	ID3D12RootSignature* RootSig = nullptr;
	wstring GS = L"", HS = L"", DS = L"";

	bool CullNone = false;
	bool CullFront = false;
	bool DisableDSV = false;
	bool DisableDSVWriting = false;
	bool EnableDSVWritingForce = false;
	bool IsDepthShader = false;
	bool DisableStencil = false;
	bool NoRTV = false;
	bool ForceCompFuncLE = false;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	DXGI_FORMAT RTVFormat = SettingsManager::ms_DX12.RTVFormat;
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_D32_FLOAT;

	float SlopeScaleDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	float DepthBias = 0.0f;

	vector<string> Permutations;
};

class Shader
{
public:
	void Init(ID3D12Device2* device, const ShaderArgs& args);

	void Compile(ID3D12Device2* device, bool exitOnFail = false);
	void CompileAllPermutations(vector<ComPtr<ID3DBlob>>& vBlobs, vector<ComPtr<ID3DBlob>>& pBlobs, vector<ComPtr<ID3DBlob>>& gBlobs, vector<size_t>& hashes, bool exitOnFail);
	void TryHotReload(ID3D12Device2* device);

	ComPtr<ID3D12PipelineState> GetPSO();
	ComPtr<ID3D12PipelineState> GetPSO(const vector<string>& permutations);
	ComPtr<ID3D12PipelineState> GetPSO(const size_t& permHash);

	bool IsPreCompiled() const;
	bool IsInitialised() const;

	wstring m_VSName, m_PSName, m_HSName, m_DSName, m_GSName;

protected:
	ComPtr<ID3DBlob> CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target, bool exitOnFail, D3D_SHADER_MACRO* permutations);

	ComPtr<ID3DBlob> ReadPreCompiledShader(LPCWSTR path);

private:
	vector<ComPtr<ID3D12PipelineState>> m_psos;
	unordered_map<size_t, size_t> m_permutationMap; // Hash to pso index

	std::chrono::system_clock::duration m_lastVSTime, m_lastPSTime, m_lastGSTime;

	bool m_preCompiled = false;
	ShaderArgs m_args;
	bool m_initialised = false;
};

