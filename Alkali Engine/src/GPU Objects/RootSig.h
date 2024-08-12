#pragma once

#include "pch.h"

using std::string;
using std::wstring;

struct RootParamInfo
{
	int NumCBV_PerFrame = 0;
	int NumCBV_PerDraw = 0;
	int NumSRV = 0;
	int NumSRV_Dynamic = 0;

	int ParamIndexCBV_PerFrame = -1;
	int ParamIndexCBV_PerDraw = -1;
	int ParamIndexSRV = -1;
	int ParamIndexSRV_Dynamic = -1;
};

class RootSig
{
public:
	RootSig();
	~RootSig();

	void Init(const string& name, const RootParamInfo& rpi, const D3D12_STATIC_SAMPLER_DESC* samplerDesc, int samplerCount);
	void Init(const string& name, const RootParamInfo& rpi);

	ID3D12RootSignature* GetRootSigResource();
	RootParamInfo& GetRootParamInfo();
	string& GetName();

private:
	ComPtr<ID3D12RootSignature> m_rootSigResource;
	RootParamInfo m_rpi;
	string m_name;
};

