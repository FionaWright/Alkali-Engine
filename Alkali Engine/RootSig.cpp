#include "pch.h"
#include "RootSig.h"
#include "ResourceManager.h"

using std::vector;

RootSig::RootSig()
	: m_rootSigResource(nullptr)
	, m_rpi({})
	, m_name("Uninitialised")
{
}

RootSig::~RootSig()
{
}

void RootSig::Init(const string& name, const RootParamInfo& rpi, D3D12_STATIC_SAMPLER_DESC* samplerDesc, int samplerCount)
{
	m_rpi = rpi;
	m_name = name + " [" + std::to_string(rpi.NumCBV_PerDraw) + "," + std::to_string(rpi.NumCBV_PerFrame) + "," + std::to_string(rpi.NumSRV) + "," + std::to_string(rpi.ParamIndexCBV_PerDraw) + "," + std::to_string(rpi.ParamIndexCBV_PerFrame) + "," + std::to_string(rpi.ParamIndexSRV) + "]";

	int paramTypesCount = (rpi.NumCBV_PerDraw != 0 ? 1 : 0) + (rpi.NumCBV_PerFrame != 0 ? 1 : 0) + (rpi.NumSRV != 0 ? 1 : 0);
	
	vector<CD3DX12_ROOT_PARAMETER1> rootParameters(paramTypesCount);

	if (rpi.ParamIndexCBV_PerDraw != -1)
	{
		CD3DX12_DESCRIPTOR_RANGE1 range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rpi.NumCBV_PerDraw, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[rpi.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);
	}	

	if (rpi.ParamIndexCBV_PerFrame != -1)
	{
		int shaderRegisterFrameStart = rpi.NumCBV_PerDraw;

		CD3DX12_DESCRIPTOR_RANGE1 range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rpi.NumCBV_PerFrame, shaderRegisterFrameStart, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[rpi.ParamIndexCBV_PerFrame].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);
	}

	if (rpi.ParamIndexSRV != -1)
	{
		CD3DX12_DESCRIPTOR_RANGE1 range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rpi.NumSRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[rpi.ParamIndexSRV].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	m_rootSigResource = ResourceManager::CreateRootSignature(rootParameters.data(), paramTypesCount, samplerDesc, samplerCount);

	wstring wstr(name.begin(), name.end());
	m_rootSigResource->SetName(wstr.c_str());
}

void RootSig::Init(const string& name, const RootParamInfo& rpi)
{
	Init(name, rpi, nullptr, 0);
}

void RootSig::InitDefaultSampler(const string& name, const RootParamInfo& rpi)
{
	D3D12_STATIC_SAMPLER_DESC sampler[1];
	sampler[0].Filter = DEFAULT_SAMPLER_FILTER;
	sampler[0].AddressU = DEFAULT_SAMPLER_ADDRESS_MODE;
	sampler[0].AddressV = DEFAULT_SAMPLER_ADDRESS_MODE;
	sampler[0].AddressW = DEFAULT_SAMPLER_ADDRESS_MODE;
	sampler[0].MipLODBias = 0;
	sampler[0].MaxAnisotropy = DEFAULT_SAMPLER_MAX_ANISOTROPIC;
	sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler[0].MinLOD = 0.0f;
	sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
	sampler[0].ShaderRegister = 0;
	sampler[0].RegisterSpace = 0;
	sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	Init(name, rpi, sampler, 1);
}

ID3D12RootSignature* RootSig::GetRootSigResource()
{
	return m_rootSigResource.Get();
}

RootParamInfo& RootSig::GetRootParamInfo()
{
	return m_rpi;
}

string& RootSig::GetName()
{
	return m_name;
}
