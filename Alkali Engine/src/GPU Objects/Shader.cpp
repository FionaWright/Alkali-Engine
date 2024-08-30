#include "pch.h"
#include "Shader.h"
#include <iostream>
#include <filesystem>

void Shader::Init(ID3D12Device2* device, const ShaderArgs& args)
{
	m_args = args;

	int dotIndex = args.VS.find_last_of('.');
	m_preCompiled = args.VS.substr(dotIndex + 1, args.VS.size() - 1 - dotIndex) == L"cso";

	wstring fileLocation = m_preCompiled ? Application::GetEXEDirectoryPath() + L"\\" : g_dirPath;

	m_VSName = fileLocation + args.VS;
	if (!args.PS.empty())
		m_PSName = fileLocation + args.PS;
	if (!args.GS.empty())
		m_GSName = fileLocation + args.GS;
	if (!args.HS.empty())
		m_HSName = fileLocation + args.HS;
	if (!args.DS.empty())
		m_DSName = fileLocation + args.DS;

	Compile(device);
}

void Shader::Compile(ID3D12Device2* device, bool exitOnFail)
{
	HRESULT hr;

	vector<ComPtr<ID3DBlob>> vBlobs;
	vector<ComPtr<ID3DBlob>> pBlobs;
	vector<ComPtr<ID3DBlob>> gBlobs;
	vector<size_t> hashes;

	if (m_preCompiled)
	{
		vBlobs.push_back(ReadPreCompiledShader(m_VSName.c_str()));

		if (!m_PSName.empty())
			pBlobs.push_back(ReadPreCompiledShader(m_PSName.c_str()));

		if (!m_GSName.empty())
			gBlobs.push_back(ReadPreCompiledShader(m_GSName.c_str()));
	}
	else
	{
		CompileAllPermutations(vBlobs, pBlobs, gBlobs, hashes, exitOnFail);

		m_lastVSTime = std::filesystem::last_write_time(m_VSName).time_since_epoch();
		if (!m_PSName.empty())
			m_lastPSTime = std::filesystem::last_write_time(m_PSName).time_since_epoch();
		if (!m_GSName.empty())
			m_lastGSTime = std::filesystem::last_write_time(m_GSName).time_since_epoch();
	}

	bool noRTV = m_args.NoRTV || m_PSName.empty();
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = noRTV ? 0 : 1;
	if (!noRTV)
		rtvFormats.RTFormats[0] = m_args.RTVFormat;

	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = SettingsManager::ms_Dynamic.WireframeMode ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	if (!SettingsManager::ms_Dynamic.CullBackFaceEnabled || m_args.CullNone)
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	else if (m_args.CullFront)
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
	else
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = m_args.DepthBias;
	rasterizerDesc.DepthBiasClamp = m_args.DepthBias > 0 ? 1.0f : 0.0f;
	rasterizerDesc.SlopeScaledDepthBias = m_args.SlopeScaleDepthBias;
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
	depthStencilDesc.DepthEnable = m_args.DisableDSV ? FALSE : TRUE;

	bool disableDepthWrite = (m_args.DisableDSVWriting || (SettingsManager::ms_Dynamic.DepthPrePassEnabled && !m_args.IsDepthShader)) && !m_args.EnableDSVWritingForce;
	depthStencilDesc.DepthWriteMask = disableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;

	if (disableDepthWrite && !m_args.ForceCompFuncLE)
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	else if (m_args.ForceCompFuncLE)
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	else
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	bool disableStencil = m_args.DisableStencil || (SettingsManager::ms_Dynamic.DepthPrePassEnabled && !m_args.IsDepthShader);
	depthStencilDesc.StencilEnable = disableStencil ? FALSE : TRUE;
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

	UINT inputLayoutCount = static_cast<UINT>(m_args.InputLayout.size());

	// For full list of fields (Order matters!)
	// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_pipeline_state_subobject_type
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_GS GS;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	} psoStream;

	psoStream.pRootSignature = m_args.RootSig;
	psoStream.InputLayout = { m_args.InputLayout.data(), inputLayoutCount };
	psoStream.PrimitiveTopologyType = m_args.Topology;
	psoStream.Blend = CD3DX12_BLEND_DESC(blendDesc);
	psoStream.RasterizerState = CD3DX12_RASTERIZER_DESC(rasterizerDesc);
	psoStream.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(depthStencilDesc);
	psoStream.DSVFormat = m_args.DSVFormat;
	psoStream.RTVFormats = rtvFormats;

	m_psos.clear();

	for (size_t i = 0; i < vBlobs.size(); i++)
	{
		psoStream.VS = CD3DX12_SHADER_BYTECODE(vBlobs[i].Get());
		if (pBlobs.size() > 0)
			psoStream.PS = CD3DX12_SHADER_BYTECODE(pBlobs[i].Get());
		if (gBlobs.size() > 0)
			psoStream.GS = CD3DX12_SHADER_BYTECODE(gBlobs[i].Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(PipelineStateStream), &psoStream };

		ComPtr<ID3D12PipelineState> pso;
		hr = device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pso));
		ThrowIfFailed(hr);

		m_psos.push_back(pso);
		if (!m_preCompiled)
			m_permutationMap.emplace(hashes[i], i);
	}

	m_initialised = true;
}

size_t HashStrVector(const vector<string>& vec) {
	size_t seed = 0;
	std::hash<string> hasher;

	for (const auto& str : vec)
		seed ^= hasher(str) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

	return seed;
}

void Shader::CompileAllPermutations(vector<ComPtr<ID3DBlob>>& vBlobs, vector<ComPtr<ID3DBlob>>& pBlobs, vector<ComPtr<ID3DBlob>>& gBlobs, vector<size_t>& hashes, bool exitOnFail) 
{
	size_t n = m_args.Permutations.size();

	assert(n <= 64 && "You cannot have more than 64 permutations");

	uint64_t combinations = pow(2, n);

	for (uint64_t bitPattern = 0; bitPattern < combinations; bitPattern++)
	{
		vector<D3D_SHADER_MACRO> macros;
		vector<string> strVector;
		for (size_t bit = 0; bit < n; bit++)
		{
			if ((bitPattern >> bit) & 1)
			{
				macros.push_back({ m_args.Permutations[bit].c_str(), "1" });
				strVector.push_back(m_args.Permutations[bit]);
			}				
		}
		macros.push_back({ NULL, NULL }); // Null terminated array
		hashes.push_back(HashStrVector(strVector));

		vBlobs.push_back(CompileShader(m_VSName.c_str(), "main", "vs_5_1", exitOnFail, macros.data()));

		if (!m_PSName.empty())
			pBlobs.push_back(CompileShader(m_PSName.c_str(), "main", "ps_5_1", exitOnFail, macros.data()));

		if (!m_GSName.empty())
			gBlobs.push_back(CompileShader(m_GSName.c_str(), "main", "gs_5_1", exitOnFail, macros.data()));
	}
}

void Shader::TryHotReload(ID3D12Device2* device)
{
	if (m_preCompiled)
		return;

	auto vsTime = std::filesystem::last_write_time(m_VSName).time_since_epoch();
	bool hotReload = (vsTime - m_lastVSTime).count() > 1;

	if (!m_PSName.empty())
	{
		auto psTime = std::filesystem::last_write_time(m_PSName).time_since_epoch();
		hotReload |= (psTime - m_lastPSTime).count() > 1;
	}		

	if (!m_GSName.empty())
	{
		auto gsTime = std::filesystem::last_write_time(m_GSName).time_since_epoch();
		hotReload |= (gsTime - m_lastGSTime).count() > 1;
	}	

	if (hotReload)
		Compile(device, true);
}

ComPtr<ID3D12PipelineState> Shader::GetPSO()
{
	return m_psos.at(0);
}

ComPtr<ID3D12PipelineState> Shader::GetPSO(const vector<string>& permutations)
{
	if (m_preCompiled)
		return m_psos.at(0);

	vector<string> existingPerms;
	for (int i = 0; i < permutations.size(); i++)
	{
		if (std::find(m_args.Permutations.begin(), m_args.Permutations.end(), permutations[i]) != m_args.Permutations.end())
			existingPerms.push_back(permutations[i]);
	}

	size_t hash = HashStrVector(existingPerms);
	if (!m_permutationMap.contains(hash))
		return m_psos.at(0);

	size_t index = m_permutationMap.at(hash);
	return m_psos.at(index);
}

ComPtr<ID3D12PipelineState> Shader::GetPSO(const size_t& permHash)
{
	if (m_preCompiled)
		return m_psos.at(0);

	if (!m_permutationMap.contains(permHash))
		return m_psos.at(0);

	size_t index = m_permutationMap.at(permHash);
	return m_psos.at(index);
}

bool Shader::IsPreCompiled() const
{
	return m_preCompiled;
}

bool Shader::IsInitialised() const
{
	return m_initialised && m_psos.size() > 0;
}

ComPtr<ID3DBlob> Shader::CompileShader(LPCWSTR path, LPCSTR mainName, LPCSTR target, bool exitOnFail, D3D_SHADER_MACRO* permutations)
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;

	UINT optimizationFlag = SettingsManager::ms_Dynamic.ShaderCompilerOptimizationEnabled ? SettingsManager::ms_Dynamic.ShaderOptimizationLevelFlag : D3DCOMPILE_SKIP_OPTIMIZATION;
	UINT warningsFlag = SettingsManager::ms_DX12.ShaderCompilationWarningsAsErrors ? D3DCOMPILE_WARNINGS_ARE_ERRORS : 0;
	UINT ieeeFlag = SettingsManager::ms_DX12.ShaderCompilationIEEEStrict ? D3DCOMPILE_IEEE_STRICTNESS : 0;
	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | optimizationFlag | warningsFlag | ieeeFlag;
	UINT effectFlags = 0; // Does nothing as of 2024

#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG;
#endif

	HRESULT hr = D3DCompileFromFile(path, permutations, D3D_COMPILE_STANDARD_FILE_INCLUDE, mainName, target, compileFlags, effectFlags, &shaderBlob, &errorBlob);

	if (FAILED(hr))
	{
		if (exitOnFail)
			return nullptr;

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
