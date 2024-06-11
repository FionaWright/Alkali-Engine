#pragma once

#include "pch.h"
#include "CBuffers.h"

// I don't use scoped enums (C++11) to avoid the explicit cast that is required to 
// treat these as root indices.
namespace GenerateMips
{
    enum
    {
        GenerateMipsCB,
        SrcMip,
        OutMip,
        NumRootParameters
    };
}

class GenerateMipsPSO
{
public:
    GenerateMipsPSO(ID3D12Device2* device);
    ~GenerateMipsPSO();

    ID3D12RootSignature* MakeCSRootSig(ID3D12Device* device, D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData);

    ID3D12RootSignature* GetRootSignature() const
    {
        return m_RootSignature.Get();
    }

    ID3D12PipelineState* GetPipelineState() const
    {
        return m_PipelineState.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultUAV() const
    {
        return m_DefaultUAV->GetCPUDescriptorHandleForHeapStart();
    }

private:
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PipelineState;

    // Default (no resource) UAV's to pad the unused UAV descriptors.
    // If generating less than 4 mip map levels, the unused mip maps
    // need to be padded with default UAVs (to keep the DX12 runtime happy).
    ComPtr<ID3D12DescriptorHeap> m_DefaultUAV;
};