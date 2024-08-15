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

void RootSig::Init(const string& name, const RootParamInfo& rpi, const D3D12_STATIC_SAMPLER_DESC* samplerDesc, int samplerCount)
{
	m_rpi = rpi;
	m_name = name + " [" + 
		std::to_string(rpi.NumCBV_PerDraw) + 
		"," + std::to_string(rpi.NumCBV_PerFrame) + 
		"," + std::to_string(rpi.NumSRV) + 
		"," + std::to_string(rpi.NumSRV_Dynamic) + 
		"," + std::to_string(rpi.ParamIndexCBV_PerDraw) + 
		"," + std::to_string(rpi.ParamIndexCBV_PerFrame) + 
		"," + std::to_string(rpi.ParamIndexSRV) + 
		"," + std::to_string(rpi.ParamIndexSRV_Dynamic) + "]";

	int paramTypesCount = (rpi.NumCBV_PerDraw != 0 ? 1 : 0) + (rpi.NumCBV_PerFrame != 0 ? 1 : 0) + (rpi.NumSRV != 0 ? 1 : 0) + (rpi.NumSRV_Dynamic != 0 ? 1 : 0);
	
	vector<CD3DX12_ROOT_PARAMETER1> rootParameters(paramTypesCount);

	CD3DX12_DESCRIPTOR_RANGE1 rangeDraw, rangeFrame, rangeSRV, rangeSRVDynamic;

	int startRegister = 0;

	if (rpi.ParamIndexCBV_PerDraw != -1)
	{		
		rangeDraw.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rpi.NumCBV_PerDraw, startRegister, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[rpi.ParamIndexCBV_PerDraw].InitAsDescriptorTable(1, &rangeDraw, D3D12_SHADER_VISIBILITY_ALL);
	}	

	if (rpi.ParamIndexCBV_PerFrame != -1)
	{
		rangeFrame.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rpi.NumCBV_PerFrame, startRegister, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[rpi.ParamIndexCBV_PerFrame].InitAsDescriptorTable(1, &rangeFrame, D3D12_SHADER_VISIBILITY_ALL);
	}

	if (rpi.ParamIndexSRV != -1)
	{
		rangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rpi.NumSRV, startRegister, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[rpi.ParamIndexSRV].InitAsDescriptorTable(1, &rangeSRV, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	if (rpi.ParamIndexSRV_Dynamic != -1)
	{
		rangeSRVDynamic.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rpi.NumSRV_Dynamic, startRegister, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParameters[rpi.ParamIndexSRV_Dynamic].InitAsDescriptorTable(1, &rangeSRVDynamic, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	m_rootSigResource = ResourceManager::CreateRootSignature(rootParameters.data(), paramTypesCount, samplerDesc, samplerCount);

	wstring wstr(name.begin(), name.end());
	m_rootSigResource->SetName(wstr.c_str());
}

void RootSig::Init(const string& name, const RootParamInfo& rpi)
{
	Init(name, rpi, nullptr, 0);
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
