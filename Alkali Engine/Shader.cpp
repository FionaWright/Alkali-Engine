#include "pch.h"
#include "Shader.h"
#include <iostream>

bool Shader::ms_GlobalFillWireframeMode;
bool Shader::ms_GlobalCullNone;

void Shader::Init(ID3D12Device2* device, const ShaderArgs& args)
{
	m_args = args;

	m_VSName = g_dirPath + args.vs;
	m_PSName = g_dirPath + args.ps;

	Compile(device);
}

void Shader::InitPreCompiled(ID3D12Device2* device, const ShaderArgs& args)
{
	m_args = args;

	m_preCompiled = true;

	m_VSName = Application::GetEXEDirectoryPath() + L"\\" + args.vs;
	m_PSName = Application::GetEXEDirectoryPath() + L"\\" + args.ps;

	Compile(device);
}

void Shader::Compile(ID3D12Device2* device)
{
	HRESULT hr;

	ComPtr<ID3DBlob> vBlob;
	ComPtr<ID3DBlob> pBlob;

	if (m_preCompiled)
	{
		vBlob = ReadPreCompiledShader(m_VSName.c_str());

		if (!m_args.NoPS)
			pBlob = ReadPreCompiledShader(m_PSName.c_str());
	}
	else
	{
		vBlob = CompileShader(m_VSName.c_str(), "main", "vs_5_1");
		if (!m_args.NoPS)
			pBlob = CompileShader(m_PSName.c_str(), "main", "ps_5_1");
	}

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = m_args.NoPS ? 0 : 1;
	if (!m_args.NoPS)
		rtvFormats.RTFormats[0] = m_args.RTVFormat;

	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = ms_GlobalFillWireframeMode ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	if (ms_GlobalCullNone || m_args.cullNone)
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	else if (m_args.cullFrontOnly)
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
	else
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,                           // BlendEnable
		FALSE,                          // LogicOpEnable
		D3D12_BLEND_SRC_ALPHA,          // SrcBlend
		D3D12_BLEND_INV_SRC_ALPHA,      // DestBlend
		D3D12_BLEND_OP_ADD,             // BlendOp
		D3D12_BLEND_ONE,                // SrcBlendAlpha
		D3D12_BLEND_ZERO,               // DestBlendAlpha
		D3D12_BLEND_OP_ADD,             // BlendOpAlpha
		D3D12_LOGIC_OP_NOOP,            // LogicOp
		D3D12_COLOR_WRITE_ENABLE_ALL    // RenderTargetWriteMask
	};
	blendDesc.RenderTarget[0] = defaultRenderTargetBlendDesc;	

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = m_args.disableDSV ? FALSE : TRUE;
	depthStencilDesc.DepthWriteMask = m_args.cullNone || m_args.disableDSVWrite ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = m_args.disableDSVWrite ? D3D12_COMPARISON_FUNC_LESS_EQUAL : D3D12_COMPARISON_FUNC_LESS;

	depthStencilDesc.StencilEnable = m_args.cullNone || m_args.disableDSV ? FALSE : TRUE;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.StencilReadMask = 0xFF;

	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	UINT inputLayoutCount = static_cast<UINT>(m_args.inputLayout.size());

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
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	} psoStream;

	psoStream.pRootSignature = m_args.rootSig;
	psoStream.InputLayout = { m_args.inputLayout.data(), inputLayoutCount };
	psoStream.PrimitiveTopologyType = m_args.Topology;
	psoStream.VS = CD3DX12_SHADER_BYTECODE(vBlob.Get());
	if (!m_args.NoPS)
		psoStream.PS = CD3DX12_SHADER_BYTECODE(pBlob.Get());
	psoStream.Blend = CD3DX12_BLEND_DESC(blendDesc);
	psoStream.RasterizerState = CD3DX12_RASTERIZER_DESC(rasterizerDesc);
	psoStream.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(depthStencilDesc);
	psoStream.DSVFormat = SettingsManager::ms_DX12.DSVFormat;
	psoStream.RTVFormats = rtvFormats;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(PipelineStateStream), &psoStream };

	hr = device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pso));
	ThrowIfFailed(hr);
}

void Shader::Recompile(ID3D12Device2* device)
{
	Compile(device);
}

ComPtr<ID3D12PipelineState> Shader::GetPSO()
{
	return m_pso;
}

bool Shader::IsPreCompiled()
{
	return m_preCompiled;
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
