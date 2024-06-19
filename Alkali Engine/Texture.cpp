#include "pch.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "TextureLoader.h"

#define BLOCK_SIZE 8
constexpr int NUM_CHANNELS = 4;

Texture::Texture()
    : m_textureData(0)
{
}

Texture::~Texture()
{
    if (m_textureResource)
    {
        m_textureResource->Release();
        m_textureResource.Reset();
    }

    if (m_textureData)
    {
        delete m_textureData;
        m_textureData = 0;
    }
}

void Texture::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath)
{
    bool _;
    Init(d3d, commandListDirect, filePath, _);
}

void Texture::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool& hasAlpha)
{
    hasAlpha = false;

    HRESULT hr;
    auto device = d3d->GetDevice();

    size_t dotIndex = filePath.find_last_of('.');
    if (dotIndex == std::string::npos)
        throw new std::exception("Invalid file path");

    string fileExtension = filePath.substr(dotIndex + 1, filePath.size() - dotIndex - 1);
    if (fileExtension == "tga")
        TextureLoader::LoadTGA(filePath, m_textureWidth, m_textureHeight, &m_textureData);
    else if (fileExtension == "dds")
        TextureLoader::LoadDDS(filePath, hasAlpha, m_textureWidth, m_textureHeight, &m_textureData, m_is2Channel);
    else
        throw new std::exception(("Invalid texture file type: ." + fileExtension).c_str());

    // NPOT dimensions not currently supported for mip mapping
    bool dimensionsNotPowerOf2 = ((m_textureWidth & (m_textureWidth - 1)) != 0) || ((m_textureHeight & (m_textureHeight - 1)) != 0);
    bool invalidForMipMaps = m_textureWidth < 8 || m_textureHeight < 8 || dimensionsNotPowerOf2;

    m_textureDesc = {};
    m_textureDesc.MipLevels = invalidForMipMaps ? 1 : GLOBAL_MIP_LEVELS;
    m_textureDesc.Format = m_is2Channel ? TEXTURE_NORMAL_MAP_DXGI_FORMAT : TEXTURE_DIFFUSE_DXGI_FORMAT;
    m_textureDesc.Width = m_textureWidth;
    m_textureDesc.Height = m_textureHeight;
    m_textureDesc.Flags = USE_SRGB ? D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_textureDesc.DepthOrArraySize = 1;
    m_textureDesc.SampleDesc.Count = 1;
    m_textureDesc.SampleDesc.Quality = 0;
    m_textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &m_textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_textureResource));
    ThrowIfFailed(hr);

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureResource.Get(), 0, 1);

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_textureUploadHeap));
    ThrowIfFailed(hr);

    int numChannels = m_is2Channel ? 2 : NUM_CHANNELS;
    int channelsPerRow = m_textureWidth * numChannels;
    int bytesPerRow = channelsPerRow * sizeof(uint8_t);
    int totalBytes = bytesPerRow * m_textureHeight;

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = m_textureData;
    textureData.RowPitch = bytesPerRow;
    textureData.SlicePitch = totalBytes;

    m_textureResource->SetName(L"Sample Texture");

    UpdateSubresources(commandListDirect, m_textureResource.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureData);

    delete m_textureData;
    m_textureData = 0;

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        TextureLoader::CreateMipMaps(d3d, commandListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        return;
    }

    ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Texture::AddToDescriptorHeap(D3DClass* d3d, ID3D12DescriptorHeap* materialTexHeap, int srvHeapOffset)
{   
    auto device = d3d->GetDevice();

    UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = materialTexHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = materialTexHeap->GetGPUDescriptorHandleForHeapStart();    

    // Assign SRV to material heap
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = m_textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_textureDesc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cpuHandle, srvHeapOffset, incrementSize);
        device->CreateShaderResourceView(m_textureResource.Get(), &srvDesc, srvHandle);
        m_srvGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, srvHeapOffset, incrementSize);
    }    
}