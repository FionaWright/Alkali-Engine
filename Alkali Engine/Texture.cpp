#include "pch.h"
#include "Texture.h"
#include "ResourceManager.h"
#include <iostream>
#include <fstream>

using std::ifstream;

Texture::Texture()
{
}

Texture::~Texture()
{
    if (m_textureData)
    {
        delete m_textureData;
        m_textureData = 0;
    }
}

void Texture::Init(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList2> commandListDirect, string filePath)
{
    HRESULT hr;

    // TODO: Check file extension to tell which function to use
    LoadTGA(filePath);

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.Width = m_textureWidth;
    textureDesc.Height = m_textureHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture));
    ThrowIfFailed(hr);

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_textureUploadHeap));
    ThrowIfFailed(hr);

    int channelsPerRow = m_textureWidth * 4;
    int bytesPerRow = channelsPerRow * sizeof(uint8_t);
    int totalBytes = bytesPerRow * m_textureHeight;

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = m_textureData;
    textureData.RowPitch = bytesPerRow;
    textureData.SlicePitch = totalBytes;

    m_texture->SetName(L"Sample Texture");

    UpdateSubresources(commandListDirect.Get(), m_texture.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureData);
    ResourceManager::TransitionResource(commandListDirect, m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    //delete m_textureData;
    //m_textureData = 0;
}

void Texture::AddToDescriptorHeap(ComPtr<ID3D12Device2> device, ComPtr<ID3D12DescriptorHeap> srvHeap)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Texture::LoadTGA(string filePath)
{
    ifstream fin;
    string longPath = "Assets/Textures/" + filePath;
    fin.open(longPath, std::ios::binary);

    if (!fin)
        throw new std::exception("IO Exception");

    struct TargaHeader
    {
        unsigned char data1[12];
        unsigned short width;
        unsigned short height;
        unsigned char bitsPerPixel;
        unsigned char data2;
    } header;

    fin.read(reinterpret_cast<char*>(&header), sizeof(TargaHeader));

    m_textureWidth = header.width;
    m_textureHeight = header.height;
    char bytesPerPixel = header.bitsPerPixel / 8;

    int totalPixelCount = m_textureWidth * m_textureHeight;
    int totalChannelCount = totalPixelCount * 4;
    m_textureData = new uint8_t[totalChannelCount];

    char* targaData = new char[totalChannelCount];
    fin.read(targaData, sizeof(char) * totalChannelCount);

    int dPixelIndex = 0;
    
    int sPixelIndex = (totalPixelCount * bytesPerPixel) - (m_textureWidth * bytesPerPixel);

    bool flipUpsideDown = false;

    // Now copy the targa image data into the targa destination array in the correct order since the targa format is stored upside down and also is not in RGBA order.
    for (int h = 0; h < m_textureHeight; h++)
    {
        for (int w = 0; w < m_textureWidth; w++)
        {
            int spi = flipUpsideDown ? (h * m_textureWidth + w) * bytesPerPixel : sPixelIndex;            

            m_textureData[dPixelIndex + 0] = static_cast<uint8_t>(targaData[spi + 2]);  // Red.
            m_textureData[dPixelIndex + 1] = static_cast<uint8_t>(targaData[spi + 1]);  // Green.
            m_textureData[dPixelIndex + 2] = static_cast<uint8_t>(targaData[spi + 0]);  // Blue

            if (bytesPerPixel == 4)
                m_textureData[dPixelIndex + 3] = static_cast<uint8_t>(targaData[spi + 3]);  // Alpha
            else
                m_textureData[dPixelIndex + 3] = 255u;  // Alpha

            sPixelIndex += bytesPerPixel;
            dPixelIndex += 4;
        }

        sPixelIndex -= (m_textureWidth * bytesPerPixel * 2);
    }

    delete[] targaData;
    targaData = 0;

    fin.close();
}

void Texture::LoadDDS()
{
}

void Texture::LoadPNG()
{
}

void Texture::LoadJPG()
{
}
