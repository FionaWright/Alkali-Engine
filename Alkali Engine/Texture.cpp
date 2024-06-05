#include "pch.h"
#include "Texture.h"
#include "ResourceManager.h"
#include <iostream>
#include <fstream>

using std::ifstream;

constexpr int NUM_CHANNELS = 4;

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
    int dotIndex = filePath.find_last_of('.');
    string fileExtension = filePath.substr(dotIndex + 1, filePath.size() - dotIndex - 1);
    if (fileExtension == "tga")
        LoadTGA(filePath);
    else if (fileExtension == "dds")
        LoadDDS(filePath);
    else
        throw new std::exception(("Invalid texture file type: ." + fileExtension).c_str());

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

    delete m_textureData;
    m_textureData = 0;
}

void Texture::AddToDescriptorHeap(ComPtr<ID3D12Device2> device, ComPtr<ID3D12DescriptorHeap> srvHeap, int srvHeapOffset)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), srvHeapOffset, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
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
    int totalChannelCount = totalPixelCount * NUM_CHANNELS;
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
            dPixelIndex += NUM_CHANNELS;
        }

        sPixelIndex -= (m_textureWidth * bytesPerPixel * 2);
    }

    delete[] targaData;
    targaData = 0;

    fin.close();
}

void Texture::LoadDDS(string filePath)
{
    ifstream fin;
    string longPath = "Assets/Textures/" + filePath;
    fin.open(longPath, std::ios::binary);

    if (!fin)
        throw new std::exception("IO Exception");

    uint32_t magicNumber;
    fin.read(reinterpret_cast<char*>(&magicNumber), sizeof(uint32_t));

    if (magicNumber != 0x20534444) // 'DDS '
        throw new std::exception("Not a valid DDS file");

    struct DDSHeader
    {
        uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwHeight;
        uint32_t dwWidth;
        uint32_t dwPitchOrLinearSize;
        uint32_t dwDepth;
        uint32_t dwMipMapCount;
        uint32_t dwReserved1[11];
        uint32_t ddspf_dwSize;
        uint32_t ddspf_dwFlags;
        uint32_t ddspf_dwFourCC;
        uint32_t ddspf_dwRGBBitCount;
        uint32_t ddspf_dwRBitMask;
        uint32_t ddspf_dwGBitMask;
        uint32_t ddspf_dwBBitMask;
        uint32_t ddspf_dwABitMask;
        uint32_t dwCaps;
        uint32_t dwCaps2;
        uint32_t dwCaps3;
        uint32_t dwCaps4;
        uint32_t dwReserved2;
    } header;

    fin.read(reinterpret_cast<char*>(&header), sizeof(DDSHeader));

    m_textureWidth = header.dwWidth;
    m_textureHeight = header.dwHeight;

    DXGI_FORMAT format;
    switch (header.ddspf_dwFourCC)
    {
    case '1TXD': // DXT1
        format = DXGI_FORMAT_BC1_UNORM;
        break;
    case '3TXD': // DXT3
        format = DXGI_FORMAT_BC2_UNORM;
        __debugbreak();
        break;
    case '5TXD': // DXT5
        format = DXGI_FORMAT_BC3_UNORM;
        __debugbreak();
        break;
    default:
        throw new std::exception("Unsupported DDS format (Uncompressed?)");
    }

    // Assuming DXT1 from here
    
    struct ColorBlock4x4
    {
        uint16_t color0;
        uint16_t color1;
        uint32_t lookupTable;
    };

    size_t totalPixelCount = m_textureWidth * m_textureHeight;
    size_t totalChannelCount = totalPixelCount * NUM_CHANNELS;
    m_textureData = new uint8_t[totalChannelCount];

    int blockWidth = m_textureWidth / 4;
    int blockHeight = m_textureHeight / 4;

    size_t totalColorBlocks = blockWidth * blockHeight;
    ColorBlock4x4* colorBlocks = new ColorBlock4x4[totalColorBlocks];

    fin.read(reinterpret_cast<char*>(colorBlocks), totalColorBlocks * sizeof(ColorBlock4x4));

    constexpr int BIT_MASK = 0b11;

    for (int bY = 0; bY < blockHeight; ++bY) // Can this be changed to just read colorBlocks linearly?
    {
        for (int bX = 0; bX < blockWidth; ++bX)
        {
            const ColorBlock4x4& block = colorBlocks[bY * blockWidth + bX];

            uint8_t r0 = (block.color0 & 0xF800) >> 8;
            uint8_t g0 = (block.color0 & 0x07E0) >> 3;
            uint8_t b0 = (block.color0 & 0x001F) << 3;

            uint8_t r1 = (block.color1 & 0xF800) >> 8;
            uint8_t g1 = (block.color1 & 0x07E0) >> 3;
            uint8_t b1 = (block.color1 & 0x001F) << 3;

            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    int blockPixelIndex = 4 * y + x;
                    int lookupTableIndex2Bit = 2 * blockPixelIndex;                    
                    uint8_t pixelColorCode = (block.lookupTable >> lookupTableIndex2Bit) & BIT_MASK;

                    uint8_t r, g, b, a;

                    if (pixelColorCode == 0b00)
                    {
                        r = r0;
                        g = g0;
                        b = b0;
                        a = 255;
                    }
                    else if (pixelColorCode == 0b01)
                    {
                        r = r1;
                        g = g1;
                        b = b1;
                        a = 255;
                    }
                    else if (pixelColorCode == 0b10 && block.color0 > block.color1)
                    {
                        r = (2 * r0 + r1) / 3;
                        g = (2 * g0 + g1) / 3;
                        b = (2 * b0 + b1) / 3;
                        a = 255;
                    }
                    else if (pixelColorCode == 0b10 && block.color0 <= block.color1)
                    {
                        r = (r0 + r1) / 2;
                        g = (g0 + g1) / 2;
                        b = (b0 + b1) / 2;
                        a = 255;
                    }
                    else if (pixelColorCode == 0b11)
                    {
                        r = (r0 + 2 * r1) / 3;
                        g = (g0 + 2 * g1) / 3;
                        b = (b0 + 2 * b1) / 3;
                        a = 255;
                    }
                    else
                        throw new std::exception("Color block error");

                    int texturePixelYIndex = bY * 4 + y;
                    int texturePixelXIndex = bX * 4 + x;
                    int pixelIndex = texturePixelYIndex * m_textureWidth + texturePixelXIndex;
                    int destIndex = pixelIndex * NUM_CHANNELS;

                    m_textureData[destIndex + 0] = r;
                    m_textureData[destIndex + 1] = g;
                    m_textureData[destIndex + 2] = b;
                    m_textureData[destIndex + 3] = a;
                }
            }
        }
    }

    delete[] colorBlocks;
    colorBlocks = 0;
    fin.close();
}

void Texture::LoadPNG(string filePath)
{
}

void Texture::LoadJPG(string filePath)
{
}
