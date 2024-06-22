#pragma once

#include "pch.h"
#include "Application.h"

using std::wstring_view;
using std::wstring;
using std::vector;

const wstring g_dirPath = L"Assets/Shaders/";

// For full list of fields (Order matters!)
	// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_pipeline_state_subobject_type
struct PipelineStateStream
{
	CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
	CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
	CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
	CD3DX12_PIPELINE_STATE_STREAM_VS VS;
	CD3DX12_PIPELINE_STATE_STREAM_PS PS;
	CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
	CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
	CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
	CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
};

class Shader
{
public:
	Shader();
	~Shader();

	void Init(const wstring& vsName, const wstring& psName, vector<D3D12_INPUT_ELEMENT_DESC> inputLayout, ID3D12RootSignature* rootSig, ID3D12Device2* device, bool cullNone = false, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	void InitPreCompiled(const wstring& vsName, const wstring& psName, vector<D3D12_INPUT_ELEMENT_DESC> inputLayout, ID3D12RootSignature* rootSig, ID3D12Device2* device, bool cullNone = false, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

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

	ID3D12RootSignature* m_rootSig;
	PipelineStateStream m_psoStream; // Remove
	ComPtr<ID3D12PipelineState> m_pso;

	bool m_preCompiled = false;
	bool m_cullNone = false;
};

