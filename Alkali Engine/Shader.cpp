#include "pch.h"
#include "Shader.h"
#include <iostream>

bool Shader::ms_FillWireframeMode;
bool Shader::ms_CullNone;

Shader::Shader()
{
}

Shader::~Shader()
{
}

void Shader::Init(const wstring& vsName, const wstring& psName, D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT inputLayoutCount, ComPtr<ID3D12RootSignature> rootSig, ComPtr<ID3D12Device2> device)
{
	m_VSName = g_dirPath + vsName;
	m_PSName = g_dirPath + psName;

	m_inputLayout = inputLayout;
	m_inputLayoutCount = inputLayoutCount;
	m_rootSig = rootSig;

	Compile(device);
}

void Shader::InitPreCompiled(const wstring& vsName, const wstring& psName, D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT inputLayoutCount, ComPtr<ID3D12RootSignature> rootSig, ComPtr<ID3D12Device2> device, wstring exePath)
{
	m_preCompiled = true;

	m_VSName = exePath + L"\\" + vsName;
	m_PSName = exePath + L"\\" + psName;

	m_inputLayout = inputLayout;
	m_inputLayoutCount = inputLayoutCount;
	m_rootSig = rootSig;

	Compile(device);
}

void Shader::Compile(ComPtr<ID3D12Device2> device)
{
	HRESULT hr;

	ComPtr<ID3DBlob> vBlob;
	ComPtr<ID3DBlob> pBlob;

	if (m_preCompiled)
	{
		vBlob = ReadPreCompiledShader(m_VSName.c_str());
		pBlob = ReadPreCompiledShader(m_PSName.c_str());
	}
	else
	{
		vBlob = CompileShader(m_VSName.c_str(), "main", "vs_5_1");
		pBlob = CompileShader(m_PSName.c_str(), "main", "ps_5_1");
	}

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = ms_FillWireframeMode ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = ms_CullNone ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// For full list of fields (Order matters!)
	// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_pipeline_state_subobject_type
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;		
	} pipelineStateStream;

	pipelineStateStream.pRootSignature = m_rootSig.Get();
	pipelineStateStream.InputLayout = { m_inputLayout, m_inputLayoutCount };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vBlob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pBlob.Get());
	pipelineStateStream.RasterizerState = CD3DX12_RASTERIZER_DESC(rasterizerDesc);
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RTVFormats = rtvFormats;	

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(PipelineStateStream), &pipelineStateStream };

	hr = device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pso));
	ThrowIfFailed(hr);
}

ComPtr<ID3D12PipelineState> Shader::GetPSO()
{
	return m_pso;
}

ComPtr<ID3DBlob> Shader::CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target)
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;

	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS; // Also see DEBUG and OPTIMIZATION flags
	UINT effectFlags = 0; // Does nothing as of 2024

#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG;
#endif

	HRESULT hr = D3DCompileFromFile(path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, mainName, target, compileFlags, effectFlags, &shaderBlob, &errorBlob);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			std::string errorMsg = static_cast<char*>(errorBlob->GetBufferPointer());
			std::wstring wideErrorMsg(errorMsg.begin(), errorMsg.end());
			wideErrorMsg = L"Shader Compilation Error: " + wideErrorMsg;
			OutputDebugStringW(wideErrorMsg.c_str());
		}			

		OutputDebugStringW(L"HRESULT ERROR: " + hr);
		ThrowIfFailed(hr);
		return nullptr;
	}

	return shaderBlob;
}

ComPtr<ID3DBlob> Shader::ReadPreCompiledShader(LPCWSTR path)
{
	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DReadFileToBlob(path, &blob);
	ThrowIfFailed(hr);
	return blob;
}
