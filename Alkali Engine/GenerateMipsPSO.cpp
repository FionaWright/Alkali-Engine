#include "pch.h"
#include "GenerateMipsPSO.h"
#include "ResourceManager.h"
#include "Application.h"

using std::string;

GenerateMipsPSO::GenerateMipsPSO(ID3D12Device2* device)
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    ComPtr<ID3DBlob> cBlob;    
    wstring path = Application::GetEXEDirectoryPath() + L"/GenerateMips.cso";
    HRESULT hr = D3DReadFileToBlob(path.c_str(), &cBlob);
    ThrowIfFailed(hr);

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    m_RootSignature = MakeCSRootSig(device, featureData);

    pipelineStateStream.pRootSignature = m_RootSignature.Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(cBlob.Get());

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(PipelineStateStream), &pipelineStateStream };

    hr = device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState));
    ThrowIfFailed(hr);

    // Create some default texture UAV's to pad any unused UAV's during mip map generation.
    //m_DefaultUAV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);
    m_DefaultUAV = ResourceManager::CreateDescriptorHeap(4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE baseHandle = m_DefaultUAV->GetCPUDescriptorHandleForHeapStart();

    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (UINT i = 0; i < 4; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = baseHandle;
        handle.ptr += i * descriptorSize;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.Texture2D.MipSlice = i;
        uavDesc.Texture2D.PlaneSlice = 0;

        device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, handle);
    }
}

GenerateMipsPSO::~GenerateMipsPSO()
{
}

ID3D12RootSignature* GenerateMipsPSO::MakeCSRootSig(ID3D12Device* device, D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData)
{
    //CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    //CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    //CD3DX12_ROOT_PARAMETER1 rootParameters[GenerateMips::NumRootParameters];
    //rootParameters[GenerateMips::GenerateMipsCB].InitAsConstants(sizeof(GenerateMipsCB) / 4, 0);
    //rootParameters[GenerateMips::SrcMip].InitAsDescriptorTable(1, &srcMip);
    //rootParameters[GenerateMips::OutMip].InitAsDescriptorTable(1, &outMip);

    // Root constants
    D3D12_ROOT_CONSTANTS rootConstants;
    rootConstants.ShaderRegister = 0; // b0
    rootConstants.RegisterSpace = 0;
    rootConstants.Num32BitValues = 6;

    // Descriptor table for SRV
    D3D12_DESCRIPTOR_RANGE srvRange;
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_DESCRIPTOR_TABLE srvTable;
    srvTable.NumDescriptorRanges = 1;
    srvTable.pDescriptorRanges = &srvRange;

    // Descriptor table for UAV
    D3D12_DESCRIPTOR_RANGE uavRange;
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 4;
    uavRange.BaseShaderRegister = 0; // u0
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_DESCRIPTOR_TABLE uavTable;
    uavTable.NumDescriptorRanges = 1;
    uavTable.pDescriptorRanges = &uavRange;

    CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
        0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    );

    // Root parameters
    D3D12_ROOT_PARAMETER rootParameters[3];

    // Root constant
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants = rootConstants;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // SRV descriptor table
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable = srvTable;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // UAV descriptor table
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable = uavTable;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root signature description
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &linearClampSampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    //CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(GenerateMips::NumRootParameters, rootParameters, 1, &linearClampSampler);

    // Serialize and create the root signature
    ComPtr<ID3DBlob> serializedRootSignature;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    ID3D12RootSignature* rootSignature;
    hr = device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create root signature");
    }

    return rootSignature;
}
