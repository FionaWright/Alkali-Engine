#include "pch.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "TextureLoader.h"

#define BLOCK_SIZE 8
constexpr int NUM_CHANNELS = 4;

Texture::Texture()
    : m_textureDesc({})
{
}

Texture::~Texture()
{
    if (m_textureResource)
    {
        m_textureResource->Release();
        m_textureResource.Reset();
    }
}

void Texture::MakeTexDesc(UINT16 arraySize)
{
    // NPOT dimensions not currently supported for mip mapping
    bool dimensionsNotPowerOf2 = ((m_textureWidth & (m_textureWidth - 1)) != 0) || ((m_textureHeight & (m_textureHeight - 1)) != 0);
    bool invalidCubemap = m_isCubemap && !SettingsManager::ms_Misc.CubemapMipMapsEnabled;
    bool invalidForMipMaps = m_textureWidth < 8 || m_textureHeight < 8 || dimensionsNotPowerOf2 || invalidCubemap;

    if (invalidForMipMaps)
        m_textureDesc.MipLevels = 1u;
    else if (SettingsManager::ms_Misc.AutoMipLevelsEnabled)
        m_textureDesc.MipLevels = static_cast<UINT16>(std::log2(std::max(m_textureWidth, m_textureHeight)) + 1.0);
    else
        m_textureDesc.MipLevels = SettingsManager::ms_Misc.DefaultGlobalMipLevels;

    m_textureDesc.Format =
        m_channels == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 3 ? DXGI_FORMAT_R8G8B8A8_UNORM :
        m_channels == 2 ? DXGI_FORMAT_R8G8_UNORM :
        DXGI_FORMAT_R8_UNORM;

    m_textureDesc.Width = m_textureWidth;
    m_textureDesc.Height = m_textureHeight;
    m_textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_textureDesc.DepthOrArraySize = arraySize;
    m_textureDesc.SampleDesc.Count = 1;
    m_textureDesc.SampleDesc.Quality = 0;
    m_textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
}

void Texture::CreateResources(ID3D12Device2* device)
{
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &m_textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_textureResource));
    ThrowIfFailed(hr);

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureResource.Get(), 0, 1) * m_textureDesc.DepthOrArraySize;

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_textureUploadHeap));
    ThrowIfFailed(hr);
}

void Texture::UploadResources(ID3D12GraphicsCommandList2* cmdListDirect, uint8_t** pData)
{
    int realChannels = m_channels == 3 ? 4 : m_channels;
    int channelsPerRow = m_textureWidth * realChannels;
    int bytesPerRow = channelsPerRow * sizeof(uint8_t);
    int totalBytes = bytesPerRow * m_textureHeight;

    UINT immediateOffset = 0;

    for (int a = 0; a < m_textureDesc.DepthOrArraySize; a++)
    {
        int mip = 0;
        UINT subresourceIndex = D3D12CalcSubresource(mip, a, 0, m_textureDesc.MipLevels, m_textureDesc.DepthOrArraySize);

        D3D12_SUBRESOURCE_DATA subresource = {};
        subresource.pData = pData[a];
        subresource.RowPitch = bytesPerRow;
        subresource.SlicePitch = totalBytes;

        UpdateSubresources(cmdListDirect, m_textureResource.Get(), m_textureUploadHeap.Get(), immediateOffset, subresourceIndex, 1, &subresource);

        immediateOffset += GetRequiredIntermediateSize(m_textureResource.Get(), subresourceIndex, 1);
    }

    for (int a = 0; a < m_textureDesc.DepthOrArraySize; a++)
        delete[] pData[a];
}

void Texture::Init(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, string filePath, bool flipUpsideDown, bool isNormalMap)
{
    auto device = d3d->GetDevice();

    m_filePath = filePath;
    string longPath = "Assets/Textures/" + filePath;

    uint8_t* pData;
    TextureLoader::LoadTex(longPath, m_textureWidth, m_textureHeight, &pData, m_hasAlpha, m_channels, flipUpsideDown, isNormalMap);

    MakeTexDesc(1);

    CreateResources(device);

    UploadResources(cmdListDirect, &pData);

    wstring debugName(filePath.begin(), filePath.end());
    m_textureResource->SetName(debugName.c_str());

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        TextureLoader::CreateMipMaps(d3d, cmdListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SettingsManager::ms_DX12.SRVFormat);
        return;
    }

    ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, SettingsManager::ms_DX12.SRVFormat);
}

void Texture::InitCubeMap(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, vector<string>& filePaths, bool flipUpsideDown)
{
    auto device = d3d->GetDevice();

    m_filePath = filePaths.at(0);
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

    MakeTexDesc(6);

    CreateResources(device);

    UploadResources(cmdListDirect, textureDatas.data());

    wstring debugName(L"Unnamed Cubemap");
    m_textureResource->SetName(debugName.c_str());

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        TextureLoader::CreateMipMapsCubemap(d3d, cmdListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SettingsManager::ms_DX12.SRVFormat);
        return;
    }

    ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, SettingsManager::ms_DX12.SRVFormat);
}

void Texture::InitCubeMapHDR(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, string filePath, bool flipUpsideDown)
{
    auto device = d3d->GetDevice();

    m_filePath = filePath;
    m_isCubemap = true;

    vector<uint8_t*> textureDatas(6);

    m_hasAlpha = true;

    string longPath = "Assets/Textures/" + filePath;
    TextureLoader::LoadHDR(longPath, m_textureWidth, m_textureHeight, textureDatas, m_channels);

    MakeTexDesc(6);

    CreateResources(device);

    UploadResources(cmdListDirect, textureDatas.data());

    wstring debugName(L"Unnamed Cubemap");
    m_textureResource->SetName(debugName.c_str());

    if (m_textureDesc.MipLevels > 1)
    {
        ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        TextureLoader::CreateMipMapsCubemap(d3d, cmdListDirect, m_textureResource.Get(), m_textureDesc);
        ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SettingsManager::ms_DX12.SRVFormat);
        return;
    }

    ResourceManager::TransitionResource(cmdListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, SettingsManager::ms_DX12.SRVFormat);
}

void Texture::InitCubeMapUAV_Empty(D3DClass* d3d)
{
    HRESULT hr;
    auto device = d3d->GetDevice();

    m_isCubemap = true;
    m_hasAlpha = true;
    m_channels = 4;
    m_textureWidth = SettingsManager::ms_Misc.IrradianceMapResolution;
    m_textureHeight = SettingsManager::ms_Misc.IrradianceMapResolution;

    MakeTexDesc(6);

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &m_textureDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_textureResource));
    ThrowIfFailed(hr);
}

void Texture::AddToDescriptorHeap(D3DClass* d3d, ID3D12DescriptorHeap* heap, size_t heapOffset)
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

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(heapStart, static_cast<INT>(heapOffset), incrementSize);
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

ID3D12Resource* Texture::GetResource()
{
    return m_textureResource.Get();
}