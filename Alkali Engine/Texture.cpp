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

void Texture::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool flipUpsideDown, bool isNormalMap)
{
    HRESULT hr;
    auto device = d3d->GetDevice();

    m_filePath = filePath;
    string longPath = "Assets/Textures/" + filePath;

    TextureLoader::LoadTex(longPath, m_textureWidth, m_textureHeight, &m_textureData, m_hasAlpha, m_channels, flipUpsideDown, isNormalMap);

    // NPOT dimensions not currently supported for mip mapping
    bool dimensionsNotPowerOf2 = ((m_textureWidth & (m_textureWidth - 1)) != 0) || ((m_textureHeight & (m_textureHeight - 1)) != 0);
    bool invalidForMipMaps = m_textureWidth < 8 || m_textureHeight < 8 || dimensionsNotPowerOf2;

    m_textureDesc = {};
    if (invalidForMipMaps)
        m_textureDesc.MipLevels = 1;
    else if (AUTO_MIP_LEVELS)
        m_textureDesc.MipLevels = std::log2(std::max(m_textureWidth, m_textureHeight)) + 1;
    else
        m_textureDesc.MipLevels = GLOBAL_MIP_LEVELS;

    DXGI_FORMAT format =
        m_channels == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 3 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 2 ? DXGI_FORMAT_R8G8_UNORM :
        DXGI_FORMAT_R8_UNORM;

    m_textureDesc.Format = format;
    m_textureDesc.Width = m_textureWidth;
    m_textureDesc.Height = m_textureHeight;
    m_textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
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

    int realChannels = m_channels == 3 ? 4 : m_channels;
    int channelsPerRow = m_textureWidth * realChannels;
    int bytesPerRow = channelsPerRow * sizeof(uint8_t);
    int totalBytes = bytesPerRow * m_textureHeight;

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = m_textureData;
    textureData.RowPitch = bytesPerRow;
    textureData.SlicePitch = totalBytes;

    wstring debugName(filePath.begin(), filePath.end());
    m_textureResource->SetName(debugName.c_str());

    UpdateSubresources(commandListDirect, m_textureResource.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureData);

    delete[] m_textureData;
    m_textureData = 0;

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        TextureLoader::CreateMipMaps(d3d, commandListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SRV_FINAL_STATE);
        return;
    }

    ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, SRV_FINAL_STATE);
}

void Texture::InitCubeMap(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, vector<string>& filePaths, bool flipUpsideDown)
{
    HRESULT hr;
    auto device = d3d->GetDevice();

    m_isCubemap = true;
    
    if (filePaths.size() != 6)
        throw std::exception("Invalid cubemap parameters");

    vector<uint8_t*> textureDatas(6);

    for (int i = 0; i < 6; i++)
    {
        string longPath = "Assets/Textures/" + filePaths[i];
        bool hasAlpha;
        int channels;
        int width, height;
        TextureLoader::LoadTex(longPath, width, height, &textureDatas[i], hasAlpha, channels, flipUpsideDown, false);

        if (i != 0)
        {
            if (hasAlpha != m_hasAlpha || channels != m_channels || width != m_textureWidth || height != m_textureHeight)
                throw std::exception("Invalid cubemap parameters");
        }
        else
        {
            m_hasAlpha = hasAlpha;
            m_channels = channels;
            m_textureWidth = width;
            m_textureHeight = height;
        }
    }

    // NPOT dimensions not currently supported for mip mapping
    bool dimensionsNotPowerOf2 = ((m_textureWidth & (m_textureWidth - 1)) != 0) || ((m_textureHeight & (m_textureHeight - 1)) != 0);
    bool invalidForMipMaps = m_textureWidth < 8 || m_textureHeight < 8 || dimensionsNotPowerOf2;

    m_textureDesc = {};
    if (invalidForMipMaps || CUBEMAP_MIP_MAPS_DISABLED)
        m_textureDesc.MipLevels = 1;
    else if (AUTO_MIP_LEVELS)
        m_textureDesc.MipLevels = std::log2(std::max(m_textureWidth, m_textureHeight)) + 1;
    else
        m_textureDesc.MipLevels = GLOBAL_MIP_LEVELS;

    DXGI_FORMAT format =
        m_channels == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 3 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 2 ? DXGI_FORMAT_R8G8_UNORM :
        DXGI_FORMAT_R8_UNORM;

    m_textureDesc.Format = format;
    m_textureDesc.Width = m_textureWidth;
    m_textureDesc.Height = m_textureHeight;
    m_textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // No cubemap flag set?
    m_textureDesc.DepthOrArraySize = 6;
    m_textureDesc.SampleDesc.Count = 1;
    m_textureDesc.SampleDesc.Quality = 0;
    m_textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &m_textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_textureResource));
    ThrowIfFailed(hr);

    int numSubresources = 6;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureResource.Get(), 0, numSubresources);

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_textureUploadHeap));
    ThrowIfFailed(hr);

    int realChannels = m_channels == 3 ? 4 : m_channels;
    int channelsPerRow = m_textureWidth * realChannels;
    int bytesPerRow = channelsPerRow * sizeof(uint8_t);
    int totalBytes = bytesPerRow * m_textureHeight;

    vector<D3D12_SUBRESOURCE_DATA> subresources(6);

    for (int i = 0; i < 6; i++)
    {
        subresources[i].pData = textureDatas[i];
        subresources[i].RowPitch = bytesPerRow;
        subresources[i].SlicePitch = totalBytes;
    }

    wstring debugName(L"Unnamed Cubemap");
    m_textureResource->SetName(debugName.c_str());

    UpdateSubresources(commandListDirect, m_textureResource.Get(), m_textureUploadHeap.Get(), 0, 0, numSubresources, subresources.data());

    for (int i = 0; i < 6; i++)
    {
        delete[] textureDatas[i];
    }

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        TextureLoader::CreateMipMaps(d3d, commandListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SRV_FINAL_STATE);
        return;
    }

    ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, SRV_FINAL_STATE);
}

void Texture::InitCubeMapHDR(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool flipUpsideDown)
{
    HRESULT hr;
    auto device = d3d->GetDevice();

    m_isCubemap = true;

    vector<uint8_t*> textureDatas(6);

    m_hasAlpha = true;

    string longPath = "Assets/Textures/" + filePath;
    TextureLoader::LoadHDR(longPath, m_textureWidth, m_textureHeight, textureDatas, m_channels);

    // NPOT dimensions not currently supported for mip mapping
    bool dimensionsNotPowerOf2 = ((m_textureWidth & (m_textureWidth - 1)) != 0) || ((m_textureHeight & (m_textureHeight - 1)) != 0);
    bool invalidForMipMaps = m_textureWidth < 8 || m_textureHeight < 8 || dimensionsNotPowerOf2;

    m_textureDesc = {};
    if (invalidForMipMaps || CUBEMAP_MIP_MAPS_DISABLED)
        m_textureDesc.MipLevels = 1;
    else if (AUTO_MIP_LEVELS)
        m_textureDesc.MipLevels = std::log2(std::max(m_textureWidth, m_textureHeight)) + 1;
    else
        m_textureDesc.MipLevels = GLOBAL_MIP_LEVELS;

    DXGI_FORMAT format =
        m_channels == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 3 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 2 ? DXGI_FORMAT_R8G8_UNORM :
        DXGI_FORMAT_R8_UNORM;

    m_textureDesc.Format = format;
    m_textureDesc.Width = m_textureWidth;
    m_textureDesc.Height = m_textureHeight;
    m_textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // No cubemap flag set?
    m_textureDesc.DepthOrArraySize = 6;
    m_textureDesc.SampleDesc.Count = 1;
    m_textureDesc.SampleDesc.Quality = 0;
    m_textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &m_textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_textureResource));
    ThrowIfFailed(hr);

    int numSubresources = 6;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureResource.Get(), 0, numSubresources);

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_textureUploadHeap));
    ThrowIfFailed(hr);

    int realChannels = m_channels == 3 ? 4 : m_channels;
    int channelsPerRow = m_textureWidth * realChannels;
    int bytesPerRow = channelsPerRow * sizeof(uint8_t);
    int totalBytes = bytesPerRow * m_textureHeight;

    vector<D3D12_SUBRESOURCE_DATA> subresources(6);

    for (int i = 0; i < 6; i++)
    {
        subresources[i].pData = textureDatas[i];
        subresources[i].RowPitch = bytesPerRow;
        subresources[i].SlicePitch = totalBytes;
    }

    wstring debugName(L"Unnamed Cubemap");
    m_textureResource->SetName(debugName.c_str());

    UpdateSubresources(commandListDirect, m_textureResource.Get(), m_textureUploadHeap.Get(), 0, 0, numSubresources, subresources.data());

    for (int i = 0; i < 6; i++)
    {
        delete[] textureDatas[i];
    }

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        TextureLoader::CreateMipMaps(d3d, commandListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SRV_FINAL_STATE);
        return;
    }

    ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, SRV_FINAL_STATE);
}

void Texture::AddToDescriptorHeap(D3DClass* d3d, ID3D12DescriptorHeap* heap, int heapOffset)
{   
    auto device = d3d->GetDevice();

    UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE heapStart = heap->GetCPUDescriptorHandleForHeapStart(); 

    // Assign SRV to material heap
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = m_textureDesc.Format;
        srvDesc.ViewDimension = m_isCubemap ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_textureDesc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(heapStart, heapOffset, incrementSize);
        device->CreateShaderResourceView(m_textureResource.Get(), &srvDesc, srvHandle);
    }    
}

bool Texture::GetHasAlpha()
{
    return m_hasAlpha;
}

string Texture::GetFilePath()
{
    return m_filePath;
}

int Texture::GetMipLevels()
{
    return m_textureDesc.MipLevels;
}

int Texture::GetChannels()
{
    return m_channels;
}
