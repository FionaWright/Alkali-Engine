#include "pch.h"
#include "Texture.h"
#include "ResourceManager.h"

#define BLOCK_SIZE 8
constexpr int NUM_CHANNELS = 4;

Texture::Texture()
    : m_textureData(0)
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
        LoadTGA(filePath);
    else if (fileExtension == "dds")
        LoadDDS(filePath, hasAlpha);
    else
        throw new std::exception(("Invalid texture file type: ." + fileExtension).c_str());

    bool isTinyTex = m_textureWidth < 8 || m_textureHeight < 8;

    m_textureDesc = {};
    m_textureDesc.MipLevels = isTinyTex ? 1 : GLOBAL_MIP_LEVELS;
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
        CreateMipMaps(d3d, commandListDirect);
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

void Texture::LoadDDS(string filePath, bool& hasAlpha)
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

    constexpr uint32_t DDPF_FOURCC = 0x4;
    bool compressed = (header.dwFlags & DDPF_FOURCC) != 0;

    if (!compressed)
    {
        __debugbreak();
        throw new std::exception("Uncompressed DDS are not yet supported");
    }

    DXGI_FORMAT format;
    switch (header.ddspf_dwFourCC)
    {
    case '1TXD': // DXT1
        format = DXGI_FORMAT_BC1_UNORM;
        LoadDDS_DXT1(fin);
        break;
    case '2TXD': // DXT2
        __debugbreak();
        throw new std::exception("DXT2 not supported");
    case '3TXD': // DXT3
        __debugbreak();
        throw new std::exception("DXT3 not supported");
    case '4TXD': // DXT4
        __debugbreak();
        throw new std::exception("DXT4 not supported");
    case '5TXD': // DXT5
        hasAlpha = true;
        format = DXGI_FORMAT_BC3_UNORM;
        LoadDDS_DXT5(fin);
        break;
    case '01XD': // DXT10
        __debugbreak();
        throw new std::exception("DX10 not supported");
    case '2ITA': // ATI2
        m_is2Channel = true;
        format = DXGI_FORMAT_BC5_UNORM;
        LoadDDS_ATI2(fin);
        break;
    default:
        throw new std::exception("Unsupported DDS format");
    }
        
    fin.close();
}

void Texture::LoadDDS_DXT1(ifstream& fin) 
{
    struct DXT1ColorBlock4x4
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
    DXT1ColorBlock4x4* colorBlocks = new DXT1ColorBlock4x4[totalColorBlocks];

    fin.read(reinterpret_cast<char*>(colorBlocks), totalColorBlocks * sizeof(DXT1ColorBlock4x4));

    constexpr int BIT_MASK = 0b11;

    constexpr bool flipUpsideDown = true;
    constexpr bool flipRightsideLeft = false;

    for (int bY = 0; bY < blockHeight; ++bY) // Can this be changed to just read colorBlocks linearly?
    {
        for (int bX = 0; bX < blockWidth; ++bX)
        {
            const DXT1ColorBlock4x4& block = colorBlocks[bY * blockWidth + bX];

            uint8_t r0 = (block.color0 & 0xF800) >> 8;
            uint8_t g0 = (block.color0 & 0x07E0) >> 3;
            uint8_t b0 = (block.color0 & 0x001F) << 3;

            uint8_t r1 = (block.color1 & 0xF800) >> 8;
            uint8_t g1 = (block.color1 & 0x07E0) >> 3;
            uint8_t b1 = (block.color1 & 0x001F) << 3;

            int dBY = flipUpsideDown ? blockHeight - 1 - bY : bY;
            int dBX = flipRightsideLeft ? blockWidth - 1 - bX : bX;

            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    int blockPixelIndex = 4 * y + x;
                    int lookupTableIndex2Bit = 2 * blockPixelIndex;
                    uint8_t pixelColorCode = (block.lookupTable >> lookupTableIndex2Bit) & BIT_MASK;

                    uint8_t r, g, b;

                    if (pixelColorCode == 0b00)
                    {
                        r = r0;
                        g = g0;
                        b = b0;
                    }
                    else if (pixelColorCode == 0b01)
                    {
                        r = r1;
                        g = g1;
                        b = b1;
                    }
                    else if (pixelColorCode == 0b10 && block.color0 > block.color1)
                    {
                        r = (2 * r0 + r1) / 3;
                        g = (2 * g0 + g1) / 3;
                        b = (2 * b0 + b1) / 3;
                    }
                    else if (pixelColorCode == 0b10 && block.color0 <= block.color1)
                    {
                        r = (r0 + r1) / 2;
                        g = (g0 + g1) / 2;
                        b = (b0 + b1) / 2;
                    }
                    else if (pixelColorCode == 0b11)
                    {
                        r = (r0 + 2 * r1) / 3;
                        g = (g0 + 2 * g1) / 3;
                        b = (b0 + 2 * b1) / 3;
                    }
                    else
                        throw new std::exception("Color block error");                    

                    int texturePixelYIndex = dBY * 4 + y;
                    int texturePixelXIndex = dBX * 4 + x;
                    int pixelIndex = texturePixelYIndex * m_textureWidth + texturePixelXIndex;
                    int destIndex = pixelIndex * NUM_CHANNELS;

                    m_textureData[destIndex + 0] = r;
                    m_textureData[destIndex + 1] = g;
                    m_textureData[destIndex + 2] = b;
                    m_textureData[destIndex + 3] = 255;
                }
            }
        }
    }

    delete[] colorBlocks;
    colorBlocks = 0;    
}

void Texture::LoadDDS_DXT5(ifstream& fin)
{
    struct DXT5AlphaBlock
    {
        uint8_t alpha0;
        uint8_t alpha1;
        uint8_t alphaLookup[6];
    };

    struct DXT5ColorBlock4x4
    {
        uint16_t color0;
        uint16_t color1;
        uint32_t colorLookup;
    };

    struct DXT5FullBlock
    {
        DXT5AlphaBlock alphaBlock;
        DXT5ColorBlock4x4 colorBlock;
    };

    size_t totalPixelCount = m_textureWidth * m_textureHeight;
    size_t totalChannelCount = totalPixelCount * NUM_CHANNELS;
    m_textureData = new uint8_t[totalChannelCount];

    int blockWidth = m_textureWidth / 4;
    int blockHeight = m_textureHeight / 4;

    size_t totalBlocks = blockWidth * blockHeight;
    DXT5FullBlock* blocks = new DXT5FullBlock[totalBlocks];

    fin.read(reinterpret_cast<char*>(blocks), totalBlocks * sizeof(DXT5FullBlock));

    constexpr int BIT_MASK = 0b11;

    constexpr bool flipUpsideDown = true;
    constexpr bool flipRightsideLeft = false;

    for (int bY = 0; bY < blockHeight; ++bY) // Can this be changed to just read colorBlocks linearly?
    {
        for (int bX = 0; bX < blockWidth; ++bX)
        {
            const DXT5FullBlock& block = blocks[bY * blockWidth + bX];

            uint8_t r0 = (block.colorBlock.color0 & 0xF800) >> 8;
            uint8_t g0 = (block.colorBlock.color0 & 0x07E0) >> 3;
            uint8_t b0 = (block.colorBlock.color0 & 0x001F) << 3;

            uint8_t r1 = (block.colorBlock.color1 & 0xF800) >> 8;
            uint8_t g1 = (block.colorBlock.color1 & 0x07E0) >> 3;
            uint8_t b1 = (block.colorBlock.color1 & 0x001F) << 3;

            uint8_t alphaValues[8];
            alphaValues[0] = block.alphaBlock.alpha0;
            alphaValues[1] = block.alphaBlock.alpha1;
            if (block.alphaBlock.alpha0 > block.alphaBlock.alpha1)
            {
                for (int i = 2; i < 8; ++i)
                    alphaValues[i] = (uint8_t)(((8 - i) * block.alphaBlock.alpha0 + (i - 1) * block.alphaBlock.alpha1) / 7);
            }
            else
            {
                for (int i = 2; i < 6; ++i)
                    alphaValues[i] = (uint8_t)(((6 - i) * block.alphaBlock.alpha0 + (i - 1) * block.alphaBlock.alpha1) / 5);
                alphaValues[6] = 0;
                alphaValues[7] = 255;
            }

            int dBY = flipUpsideDown ? blockHeight - 1 - bY : bY;
            int dBX = flipRightsideLeft ? blockWidth - 1 - bX : bX;

            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    int blockPixelIndex = 4 * y + x;
                    int lookupTableIndex2Bit = 2 * blockPixelIndex;
                    uint8_t pixelColorCode = (block.colorBlock.colorLookup >> lookupTableIndex2Bit) & BIT_MASK;

                    int alphaIndex = (block.alphaBlock.alphaLookup[blockPixelIndex / 2] >> (4 * (blockPixelIndex % 2))) & 0x0F;

                    int alphaBitPos = blockPixelIndex * 3;
                    alphaIndex = ((block.alphaBlock.alphaLookup[alphaBitPos / 8] >> (alphaBitPos % 8)) | (block.alphaBlock.alphaLookup[alphaBitPos / 8 + 1] << (8 - (alphaBitPos % 8)))) & 0x07;

                    uint8_t r, g, b;

                    if (pixelColorCode == 0b00)
                    {
                        r = r0;
                        g = g0;
                        b = b0;
                    }
                    else if (pixelColorCode == 0b01)
                    {
                        r = r1;
                        g = g1;
                        b = b1;
                    }
                    else if (pixelColorCode == 0b10)
                    {
                        r = (2 * r0 + r1) / 3;
                        g = (2 * g0 + g1) / 3;
                        b = (2 * b0 + b1) / 3;
                    }
                    else if (pixelColorCode == 0b11)
                    {
                        r = (r0 + 2 * r1) / 3;
                        g = (g0 + 2 * g1) / 3;
                        b = (b0 + 2 * b1) / 3;
                    }
                    else
                        throw new std::exception("Color block error");                    

                    int texturePixelYIndex = dBY * 4 + y;
                    int texturePixelXIndex = dBX * 4 + x;
                    int pixelIndex = texturePixelYIndex * m_textureWidth + texturePixelXIndex;
                    int destIndex = pixelIndex * NUM_CHANNELS;

                    m_textureData[destIndex + 0] = r;
                    m_textureData[destIndex + 1] = g;
                    m_textureData[destIndex + 2] = b;
                    m_textureData[destIndex + 3] = alphaValues[alphaIndex];
                }
            }
        }
    }

    delete[] blocks;
}

void decodeIndices(const uint8_t indices[6], uint8_t* outIndices)
{
    for (int i = 0; i < 16; ++i)
    {
        int index = (i * 3) / 8;
        int shift = (i * 3) % 8;
        outIndices[i] = (indices[index] >> shift) & 0x07;
        if (shift > 5)
        {
            outIndices[i] |= (indices[index + 1] << (8 - shift)) & 0x07;
        }
    }
}

void computeInterpolatedValues(uint8_t v0, uint8_t v1, uint8_t* values)
{
    values[0] = v0;
    values[1] = v1;
    if (v0 > v1)
    {
        for (int i = 2; i < 8; ++i)
        {
            values[i] = ((8 - i) * v0 + (i - 1) * v1) / 7;
        }
    }
    else
    {
        for (int i = 2; i < 6; ++i)
        {
            values[i] = ((6 - i) * v0 + (i - 1) * v1) / 5;
        }
        values[6] = 0;
        values[7] = 255;
    }
}

void Texture::LoadDDS_ATI2(ifstream& fin)
{
    struct ATI2Block
    {
        uint8_t red0;
        uint8_t red1;
        uint8_t redIndices[6];
        uint8_t green0;
        uint8_t green1;
        uint8_t greenIndices[6];
    };

    int numChannels = 2;

    size_t totalPixelCount = m_textureWidth * m_textureHeight;
    size_t totalChannelCount = totalPixelCount * numChannels;
    m_textureData = new uint8_t[totalChannelCount];

    int blockWidth = m_textureWidth / 4;
    int blockHeight = m_textureHeight / 4;

    size_t totalBlocks = blockWidth * blockHeight;
    ATI2Block* blocks = new ATI2Block[totalBlocks];

    fin.read(reinterpret_cast<char*>(blocks), totalBlocks * sizeof(ATI2Block));

    constexpr bool flipUpsideDown = true;
    constexpr bool flipRightsideLeft = false;

    uint8_t decompressedRed[16];
    uint8_t decompressedGreen[16];

    for (int bY = 0; bY < blockHeight; ++bY)
    {
        for (int bX = 0; bX < blockWidth; ++bX)
        {
            const ATI2Block& block = blocks[bY * blockWidth + bX];

            uint8_t xIndices[16], yIndices[16];
            decodeIndices(block.redIndices, xIndices);
            decodeIndices(block.greenIndices, yIndices);

            uint8_t xValues[8], yValues[8];
            computeInterpolatedValues(block.red0, block.red1, xValues);
            computeInterpolatedValues(block.green0, block.green1, yValues);

            int dBY = flipUpsideDown ? blockHeight - 1 - bY : bY;
            int dBX = flipRightsideLeft ? blockWidth - 1 - bX : bX;

            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    int blockPixelIndex = 4 * y + x;
                    int texturePixelIndex = ((dBY * 4 + y) * m_textureWidth + (dBX * 4 + x)) * 2;

                    m_textureData[texturePixelIndex + 0] = xValues[xIndices[blockPixelIndex]];
                    m_textureData[texturePixelIndex + 1] = yValues[yIndices[blockPixelIndex]];
                }
            }
        }
    }

    delete[] blocks;
}

void Texture::LoadPNG(string filePath)
{
}

void Texture::LoadJPG(string filePath)
{
}

void Texture::CreateMipMaps(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect)
{
	//Union used for shader constants
	struct DWParam
	{
		DWParam(FLOAT f) : Float(f) {}
		DWParam(UINT u) : Uint(u) {}

		void operator= (FLOAT f) { Float = f; }
		void operator= (UINT u) { Uint = u; }

		union
		{
			FLOAT Float;
			UINT Uint;
		};
	};

    bool debugMode = false;

    int mipLevels = m_textureDesc.MipLevels;
    auto device = d3d->GetDevice();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (mipLevels <= 1)
        return;

	//Calculate heap size
	uint32_t requiredHeapSize = mipLevels - 1;

	//The compute shader expects 2 floats, the source texture and the destination texture
	CD3DX12_DESCRIPTOR_RANGE srvCbvRanges[2];
	CD3DX12_ROOT_PARAMETER rootParameters[3];
	srvCbvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	srvCbvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	rootParameters[0].InitAsConstants(debugMode ? 3 : 2, 0);
	rootParameters[1].InitAsDescriptorTable(1, &srvCbvRanges[0]);
	rootParameters[2].InitAsDescriptorTable(1, &srvCbvRanges[1]);

	//Static sampler used to get the linearly interpolated color for the mipmaps
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MaxAnisotropy = 0;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	samplerDesc.ShaderRegister = 0;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//Create the root signature for the mipmap compute shader from the parameters and sampler above
	ID3DBlob* signature;
	ID3DBlob* error;
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

	ID3D12RootSignature* mipMapRootSignature;
    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mipMapRootSignature));

	//Create the descriptor heap with layout: source texture - destination texture
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 2 * requiredHeapSize;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ID3D12DescriptorHeap* descriptorHeap;
    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));	

    ComPtr<ID3DBlob> cBlob;
    wstring path = Application::GetEXEDirectoryPath() + (debugMode ? L"/CreateMipMapsDebug.cso" : L"/CreateMipMaps.cso");
    HRESULT hr = D3DReadFileToBlob(path.c_str(), &cBlob);
    ThrowIfFailed(hr);

	//Create pipeline state object for the compute shader using the root signature.
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mipMapRootSignature;
	psoDesc.CS = { reinterpret_cast<UINT8*>(cBlob->GetBufferPointer()), cBlob->GetBufferSize() };

	ID3D12PipelineState* psoMipMaps;
    device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psoMipMaps));

	//Prepare the SRV description for the source texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSRVDesc = {};
	srcTextureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srcTextureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srcTextureSRVDesc.Format = m_textureDesc.Format;
    srcTextureSRVDesc.Texture2D.MipLevels = 1;

	//Prepare the unordered access view description for the destination texture
	D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUAVDesc = {};
	destTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    destTextureUAVDesc.Format = m_textureDesc.Format;

	//Set root signature, pso and descriptor heap
    commandListDirect->SetComputeRootSignature(mipMapRootSignature);
    commandListDirect->SetPipelineState(psoMipMaps);
    commandListDirect->SetDescriptorHeaps(1, &descriptorHeap);

	//CPU handle for the first descriptor on the descriptor heap, used to fill the heap
	CD3DX12_CPU_DESCRIPTOR_HANDLE currentCPUHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, descriptorSize);

	//GPU handle for the first descriptor on the descriptor heap, used to initialize the descriptor tables
	CD3DX12_GPU_DESCRIPTOR_HANDLE currentGPUHandle(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, descriptorSize);

    //Transition from pixel shader resource to unordered access
    ResourceManager::TransitionResource(commandListDirect, m_textureResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    //Loop through the mipmaps copying from the bigger mipmap to the smaller one with downsampling in a compute shader
    for (uint32_t TopMip = 0; TopMip < mipLevels - 1; TopMip++)
    {
        //Get mipmap dimensions
        uint32_t dstWidth = std::max(m_textureWidth >> (TopMip + 1), 1);
        uint32_t dstHeight = std::max(m_textureHeight >> (TopMip + 1), 1);

        //Create shader resource view for the source texture in the descriptor heap
        srcTextureSRVDesc.Texture2D.MostDetailedMip = TopMip;
        device->CreateShaderResourceView(m_textureResource.Get(), &srcTextureSRVDesc, currentCPUHandle);
        currentCPUHandle.Offset(1, descriptorSize);

        //Create unordered access view for the destination texture in the descriptor heap        
        destTextureUAVDesc.Texture2D.MipSlice = TopMip + 1;
        device->CreateUnorderedAccessView(m_textureResource.Get(), nullptr, &destTextureUAVDesc, currentCPUHandle);
        currentCPUHandle.Offset(1, descriptorSize);

        //Pass the destination texture pixel size to the shader as constants
        commandListDirect->SetComputeRoot32BitConstant(0, DWParam(1.0f / dstWidth).Uint, 0);
        commandListDirect->SetComputeRoot32BitConstant(0, DWParam(1.0f / dstHeight).Uint, 1);
        if (debugMode)
            commandListDirect->SetComputeRoot32BitConstant(0, TopMip, 2);

        //Pass the source and destination texture views to the shader via descriptor tables
        commandListDirect->SetComputeRootDescriptorTable(1, currentGPUHandle);
        currentGPUHandle.Offset(1, descriptorSize);
        commandListDirect->SetComputeRootDescriptorTable(2, currentGPUHandle);
        currentGPUHandle.Offset(1, descriptorSize);

        //Dispatch the compute shader with one thread per 8x8 pixels
        commandListDirect->Dispatch(std::max(dstWidth / 8, 1u), std::max(dstHeight / 8, 1u), 1);

        //Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_textureResource.Get());
        commandListDirect->ResourceBarrier(1, &uavBarrier);
    }    
}