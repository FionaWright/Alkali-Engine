#include "pch.h"
#include "TextureLoader.h"
#include "Constants.h"
#include "Application.h"
#include "ResourceManager.h"
#include "Scene.h"

#include "spng/spng.h"

#include <string>
#include <filesystem>
using std::wstring;
using std::ofstream;

#pragma warning (disable : 6386)
#pragma warning (disable : 6385)

#define BLOCK_SIZE 8
constexpr int NUM_CHANNELS = 4;

constexpr bool FLIP_DXT1_UPSIDE_DOWN = true;
constexpr bool FLIP_DXT1_RIGHTSIDE_LEFT = false;
constexpr bool FLIP_DXT1_BLOCKS_UPSIDE_DOWN = true;
constexpr bool FLIP_DXT1_BLOCKS_RIGHTSIDE_LEFT = false;

constexpr bool FLIP_DXT5_UPSIDE_DOWN = true;
constexpr bool FLIP_DXT5_RIGHTSIDE_LEFT = false;
constexpr bool FLIP_DXT5_BLOCKS_UPSIDE_DOWN = true;
constexpr bool FLIP_DXT5_BLOCKS_RIGHTSIDE_LEFT = false;

constexpr bool FLIP_ATI2_UPSIDE_DOWN = true;
constexpr bool FLIP_ATI2_RIGHTSIDE_LEFT = false;
constexpr bool FLIP_ATI2_BLOCKS_UPSIDE_DOWN = true;
constexpr bool FLIP_ATI2_BLOCKS_RIGHTSIDE_LEFT = false;

constexpr bool FLIP_TGA_UPSIDE_DOWN = false;
constexpr bool FLIP_TGA_RIGHTSIDE_LEFT = false;

ID3D12RootSignature* TextureLoader::ms_mipMapRootSig, *TextureLoader::ms_mipMapRootSigCubemap;
ID3D12RootSignature* TextureLoader::ms_irradianceRootSig;
ID3D12PipelineState* TextureLoader::ms_mipMapPSO;
ID3D12PipelineState* TextureLoader::ms_mipMapPSOCubemap;
ID3D12PipelineState* TextureLoader::ms_irradiancePSO;
int TextureLoader::ms_descriptorSize;
vector<ID3D12DescriptorHeap*> TextureLoader::ms_trackedDescHeaps;
std::mutex TextureLoader::ms_mutexMipMapGen;

void TextureLoader::LoadTex(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels, bool flipUpsideDown, bool isNormalMap)
{
    size_t dotIndex = filePath.find_last_of('.');
    if (dotIndex == std::string::npos)
        throw new std::exception("Invalid file path");

    string binTexPath = filePath.substr(0, dotIndex) + ".binTex";
    bool binTexAlternativeExists = std::filesystem::exists(binTexPath);

    string fileExtension = filePath.substr(dotIndex + 1, filePath.size() - dotIndex - 1);
    if ((SettingsManager::ms_Dynamic.AllowBinTex && binTexAlternativeExists) || fileExtension == "binTex")
    {
        LoadBinTex(binTexPath, width, height, pData, hasAlpha, channels);
        return;
    }

    if (fileExtension == "tga")
    {
        LoadTGA(filePath, width, height, pData);

        hasAlpha = ManuallyDetermineHasAlpha(width * height * channels, channels, *pData);
        channels = hasAlpha ? 4 : 3;
        StoreBinTex(binTexPath, width, height, *pData, hasAlpha, channels, flipUpsideDown, isNormalMap);
        return;
    }        

    if (fileExtension == "dds")
    {
        LoadDDS(filePath, width, height, pData, hasAlpha, channels);
        StoreBinTex(binTexPath, width, height, *pData, hasAlpha, channels, flipUpsideDown, isNormalMap);
        return;
    }        

    if (fileExtension == "png")
    {        
        LoadPNG(filePath, width, height, pData, channels);

        hasAlpha = ManuallyDetermineHasAlpha(width * height * channels, channels, *pData);
        StoreBinTex(binTexPath, width, height, *pData, hasAlpha, channels, flipUpsideDown, isNormalMap);
        return;
    }        
    
    throw new std::exception(("Invalid texture file type: ." + fileExtension).c_str());
}

void TextureLoader::StoreBinTex(string filePath, int width, int height, uint8_t* pData, bool hasAlpha, int channels, bool flipUpsideDown, bool isNormalMap)
{
    ofstream fout;
    std::ios_base::openmode openFlags = std::ios::binary | std::ios::out | std::ios::trunc;
    fout.open(filePath, openFlags);
    if (!fout)
        throw new std::exception("IO Exception (OUTPUT)");

    fout.write(reinterpret_cast<const char*>(&width), sizeof(int));
    fout.write(reinterpret_cast<const char*>(&height), sizeof(int));
    fout.write(reinterpret_cast<const char*>(&hasAlpha), sizeof(bool));        

    uint8_t* dataSrc = pData;

    if (isNormalMap && channels == 3 && SettingsManager::ms_Misc.NormalMapChannels == SettingsManager::ms_Misc.ONLY_2_CHANNEL)
    {
        int newChannels = 2;
        size_t pixelCount = width * height;
        size_t destBytes = pixelCount * newChannels;

        channels = newChannels;

        dataSrc = new uint8_t[destBytes];

        for (int i = 0; i < pixelCount; i++)
        {
            std::memcpy(dataSrc + i * 2, pData + i * 4, sizeof(uint8_t) * newChannels);
        }
    }    

    if (isNormalMap && channels == 2 && SettingsManager::ms_Misc.NormalMapChannels == SettingsManager::ms_Misc.ONLY_3_CHANNEL)
    {
        int newChannels = 4;
        size_t pixelCount = width * height;
        size_t destBytes = pixelCount * newChannels;

        channels = 3;

        dataSrc = new uint8_t[destBytes];

        for (int i = 0; i < pixelCount; i++)
        {
            float R = pData[i * 2 + 0] * 2.0f - 1.0f;
            float G = pData[i * 2 + 1] * 2.0f - 1.0f;
            float Z = std::sqrt(1.0f - (R * R + G * G));
            dataSrc[i * 4 + 0] = static_cast<uint8_t>((R + 1.0f) * 0.5f);
            dataSrc[i * 4 + 1] = static_cast<uint8_t>((G + 1.0f) * 0.5f);
            dataSrc[i * 4 + 2] = static_cast<uint8_t>((Z + 1.0f) * 0.5f);
            dataSrc[i * 4 + 3] = UINT8_MAX;
        }
    }

    int physicalChannels = channels == 3 ? 4 : channels;

    fout.write(reinterpret_cast<const char*>(&channels), sizeof(int));

    size_t bytes = width * height * physicalChannels;

    if (flipUpsideDown)
    {
        uint8_t* flippedData = new uint8_t[bytes];
        size_t rowBytes = width * physicalChannels;

        for (int y = 0; y < height; y++)
        {
            std::memcpy(flippedData + y * rowBytes, dataSrc + (height - 1 - y) * rowBytes, rowBytes);
        }

        fout.write(reinterpret_cast<const char*>(flippedData), bytes);
        delete[] flippedData;
    }
    else
        fout.write(reinterpret_cast<const char*>(dataSrc), bytes);

    fout.close();

    if (!fout.good())
        throw new std::exception("IO Exception (OUTPUT)");

    if (dataSrc != pData)
        delete[] dataSrc;
}

void TextureLoader::LoadBinTex(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels)
{
    ifstream fin;
    fin.open(filePath, std::ios::binary);
    if (!fin)
        throw new std::exception("IO Exception");

    fin.read(reinterpret_cast<char*>(&width), sizeof(int));
    fin.read(reinterpret_cast<char*>(&height), sizeof(int));
    fin.read(reinterpret_cast<char*>(&hasAlpha), sizeof(bool));
    fin.read(reinterpret_cast<char*>(&channels), sizeof(int));

    int realChannels = channels == 3 ? 4 : channels;

    size_t bytes = width * height * realChannels;
    *pData = new uint8_t[bytes];
    fin.read(reinterpret_cast<char*>(*pData), bytes);
}

void TextureLoader::LoadTGA(string filePath, int& width, int& height, uint8_t** pData)
{
    ifstream fin;    
    fin.open(filePath, std::ios::binary);

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

    width = header.width;
    height = header.height;
    char bytesPerPixel = header.bitsPerPixel / 8;

    int totalPixelCount = width * height;
    int totalChannelCount = totalPixelCount * NUM_CHANNELS;
    (*pData) = new uint8_t[totalChannelCount];

    char* targaData = new char[totalChannelCount];
    fin.read(targaData, sizeof(char) * totalChannelCount);

    int dPixelIndex = 0;

    int sPixelIndex = (totalPixelCount * bytesPerPixel) - (width * bytesPerPixel);

    // Now copy the targa image data into the targa destination array in the correct order since the targa format is stored upside down and also is not in RGBA order.
    for (int h = 0; h < height; h++)
    {
        for (int w = 0; w < width; w++)
        {
            int spi = FLIP_TGA_UPSIDE_DOWN ? (h * width + w) * bytesPerPixel : sPixelIndex;

            int dpi = dPixelIndex * NUM_CHANNELS;
            (*pData)[dpi + 0] = static_cast<uint8_t>(targaData[spi + 2]);  // Red.
            (*pData)[dpi + 1] = static_cast<uint8_t>(targaData[spi + 1]);  // Green.
            (*pData)[dpi + 2] = static_cast<uint8_t>(targaData[spi + 0]);  // Blue

            if (bytesPerPixel == 4)
                (*pData)[dpi + 3] = static_cast<uint8_t>(targaData[spi + 3]);  // Alpha
            else
                (*pData)[dpi + 3] = 255u;  // Alpha

            sPixelIndex += bytesPerPixel;
            dPixelIndex++;
        }

        sPixelIndex -= (width * bytesPerPixel * 2);
    }

    delete[] targaData;
    targaData = 0;

    fin.close();
}

void TextureLoader::LoadDDS(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels)
{
    ifstream fin;
    fin.open(filePath, std::ios::binary);

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

    width = header.dwWidth;
    height = header.dwHeight;

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
        LoadDDS_DXT1(fin, width, height, pData);
        channels = 4;
        break;

    case '5TXD': // DXT5
        hasAlpha = true;
        format = DXGI_FORMAT_BC3_UNORM;
        LoadDDS_DXT5(fin, width, height, pData);
        channels = 4;
        break;

    case '2ITA': // ATI2
        format = DXGI_FORMAT_BC5_UNORM;
        LoadDDS_ATI2(fin, width, height, pData);
        channels = 2;
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
    case '01XD': // DXT10
        __debugbreak();
        throw new std::exception("DX10 not supported");
    default:
        throw new std::exception("Unsupported DDS format");
    }

    fin.close();
}

void TextureLoader::LoadDDS_DXT1(ifstream& fin, int& width, int& height, uint8_t** pData)
{
    struct DXT1ColorBlock4x4
    {
        uint16_t color0;
        uint16_t color1;
        uint32_t lookupTable;
    };

    size_t totalPixelCount = width * height;
    size_t totalChannelCount = totalPixelCount * NUM_CHANNELS;
    (*pData) = new uint8_t[totalChannelCount];

    int blockWidth = width / 4;
    int blockHeight = height / 4;

    size_t totalColorBlocks = blockWidth * blockHeight;
    DXT1ColorBlock4x4* colorBlocks = new DXT1ColorBlock4x4[totalColorBlocks];

    fin.read(reinterpret_cast<char*>(colorBlocks), totalColorBlocks * sizeof(DXT1ColorBlock4x4));

    constexpr int BIT_MASK = 0b11;

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

            int dBY = FLIP_DXT1_UPSIDE_DOWN ? blockHeight - 1 - bY : bY;
            int dBX = FLIP_DXT1_RIGHTSIDE_LEFT ? blockWidth - 1 - bX : bX;

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

                    int dY = FLIP_DXT1_BLOCKS_UPSIDE_DOWN ? 3 - y : y;
                    int dX = FLIP_DXT1_BLOCKS_RIGHTSIDE_LEFT ? 3 - x : x;

                    int texturePixelYIndex = dBY * 4 + dY;
                    int texturePixelXIndex = dBX * 4 + dX;
                    int pixelIndex = texturePixelYIndex * width + texturePixelXIndex;
                    int destIndex = pixelIndex * NUM_CHANNELS;

                    (*pData)[destIndex + 0] = r;
                    (*pData)[destIndex + 1] = g;
                    (*pData)[destIndex + 2] = b;
                    (*pData)[destIndex + 3] = 255;
                }
            }
        }
    }

    delete[] colorBlocks;
    colorBlocks = 0;
}

void TextureLoader::LoadDDS_DXT5(ifstream& fin, int& width, int& height, uint8_t** pData)
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

    size_t totalPixelCount = width * height;
    size_t totalChannelCount = totalPixelCount * NUM_CHANNELS;
    (*pData) = new uint8_t[totalChannelCount];

    int blockWidth = width / 4;
    int blockHeight = height / 4;

    size_t totalBlocks = blockWidth * blockHeight;
    DXT5FullBlock* blocks = new DXT5FullBlock[totalBlocks];

    fin.read(reinterpret_cast<char*>(blocks), totalBlocks * sizeof(DXT5FullBlock));

    constexpr int BIT_MASK = 0b11;

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

            int dBY = FLIP_DXT5_UPSIDE_DOWN ? blockHeight - 1 - bY : bY;
            int dBX = FLIP_DXT5_RIGHTSIDE_LEFT ? blockWidth - 1 - bX : bX;

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

                    int dY = FLIP_DXT5_BLOCKS_UPSIDE_DOWN ? 3 - y : y;
                    int dX = FLIP_DXT5_BLOCKS_RIGHTSIDE_LEFT ? 3 - x : x;

                    int texturePixelYIndex = dBY * 4 + dY;
                    int texturePixelXIndex = dBX * 4 + dX;
                    int pixelIndex = texturePixelYIndex * width + texturePixelXIndex;
                    int destIndex = pixelIndex * NUM_CHANNELS;

                    (*pData)[destIndex + 0] = r;
                    (*pData)[destIndex + 1] = g;
                    (*pData)[destIndex + 2] = b;
                    (*pData)[destIndex + 3] = alphaValues[alphaIndex];
                }
            }
        }
    }

    delete[] blocks;
}

void DecodeIndices(const uint8_t indices[6], uint8_t* outIndices)
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

void ComputeInterpolatedValues(uint8_t v0, uint8_t v1, uint8_t* values)
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

void TextureLoader::LoadDDS_ATI2(ifstream& fin, int& width, int& height, uint8_t** pData)
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

    size_t totalPixelCount = width * height;
    size_t totalChannelCount = totalPixelCount * numChannels;
    (*pData) = new uint8_t[totalChannelCount];

    int blockWidth = width / 4;
    int blockHeight = height / 4;

    size_t totalBlocks = blockWidth * blockHeight;
    ATI2Block* blocks = new ATI2Block[totalBlocks];

    fin.read(reinterpret_cast<char*>(blocks), totalBlocks * sizeof(ATI2Block));   

    for (int bY = 0; bY < blockHeight; ++bY)
    {
        for (int bX = 0; bX < blockWidth; ++bX)
        {
            const ATI2Block& block = blocks[bY * blockWidth + bX];

            uint8_t xIndices[16], yIndices[16];
            DecodeIndices(block.redIndices, xIndices);
            DecodeIndices(block.greenIndices, yIndices);

            uint8_t xValues[8], yValues[8];
            ComputeInterpolatedValues(block.red0, block.red1, xValues);
            ComputeInterpolatedValues(block.green0, block.green1, yValues);

            int dBY = FLIP_ATI2_UPSIDE_DOWN ? blockHeight - 1 - bY : bY;
            int dBX = FLIP_ATI2_RIGHTSIDE_LEFT ? blockWidth - 1 - bX : bX;

            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    int dY = FLIP_ATI2_BLOCKS_UPSIDE_DOWN ? 3 - y : y;
                    int dX = FLIP_ATI2_BLOCKS_RIGHTSIDE_LEFT ? 3 - x : x;

                    int blockPixelIndex = 4 * dY + dX;
                    int texturePixelIndex = ((dBY * 4 + dY) * width + (dBX * 4 + dX)) * 2;

                    (*pData)[texturePixelIndex + 0] = xValues[xIndices[blockPixelIndex]];
                    (*pData)[texturePixelIndex + 1] = yValues[yIndices[blockPixelIndex]];
                }
            }
        }
    }

    delete[] blocks;
}

int GetChannelsFromColorType(int color_type)
{
    switch (color_type)
    {
    case SPNG_COLOR_TYPE_GRAYSCALE:
        return 1; // 1 channel: grayscale
    case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
        return 2; // 2 channels: grayscale + alpha
    case SPNG_COLOR_TYPE_TRUECOLOR:
        return 3; // 3 channels: RGB
    case SPNG_COLOR_TYPE_INDEXED:
        return 3; // 3 channels: RGB (palette)
    case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
        return 4; // 4 channels: RGBA
    default:
        throw std::runtime_error("Unknown color type");
    }
}

void TextureLoader::LoadPNG(string filePath, int& width, int& height, uint8_t** pData, int& channels)
{
    FILE* file; 
    fopen_s(&file, filePath.c_str(), "rb");
    if (!file)
        throw std::exception("I/O Error");

    spng_ctx* ctx = spng_ctx_new(0);

    spng_set_png_file(ctx, file);

    spng_ihdr ihdr;
    spng_get_ihdr(ctx, &ihdr);

    width = ihdr.width;
    height = ihdr.height;
    channels = GetChannelsFromColorType(ihdr.color_type);

    if (filePath == "Assets/Textures/Transparent.png")
        channels = 4;

    spng_format format = 
        channels == 4 ? SPNG_FMT_RGBA8 :
        channels == 3 ? SPNG_FMT_RGBA8 : // No existing RGB8 DXGI format so we have to leave the alpha empty
        channels == 2 ? SPNG_FMT_GA8 : 
        SPNG_FMT_G8;

    size_t outSize;
    spng_decoded_image_size(ctx, format, &outSize);
    *pData = new uint8_t[outSize];

    spng_decode_image(ctx, *pData, outSize, format, 0);

    spng_ctx_free(ctx);
    fclose(file);
}

void TextureLoader::LoadJPG(string filePath, int& width, int& height, uint8_t** pData, int& channels)
{
}

XMFLOAT3 rgbeToFloat(const uint8_t* rgbe) 
{
    if (rgbe[3] == 0)
        return XMFLOAT3(0, 0, 0);

    float f = ldexp(1.0f, rgbe[3] - (int)(128 + 8));
    return XMFLOAT3(rgbe[0] * f, rgbe[1] * f, rgbe[2] * f);
}

XMFLOAT3 SampleHDR(const uint8_t* hdrData, const XMFLOAT3& direction, const int hdrWidth, const int hdrHeight) 
{
    float theta = atan2f(direction.z, direction.x);
    float phi = acos(direction.y);

    float texU = ((theta / (2.0f * static_cast<float>(PI))) + 0.5f) * hdrWidth;
    float texV = (phi / static_cast<float>(PI)) * hdrHeight;

    texU = std::clamp(texU, 0.0f, static_cast<float>(hdrWidth - 1));
    texV = std::clamp(texV, 0.0f, static_cast<float>(hdrHeight - 1));

    int x = static_cast<int>(texU);
    int y = static_cast<int>(texV);
    int x1 = std::min(x + 1, hdrWidth - 1);
    int y1 = std::min(y + 1, hdrHeight - 1);
    float dx = texU - x;
    float dy = texV - y;

    const uint8_t* c00 = &hdrData[(y * hdrWidth + x) * 4];
    XMFLOAT3 col00 = rgbeToFloat(c00);

    const uint8_t* c10 = &hdrData[(y * hdrWidth + x1) * 4];
    XMFLOAT3 col10 = rgbeToFloat(c10);

    const uint8_t* c01 = &hdrData[(y1 * hdrWidth + x) * 4];
    XMFLOAT3 col01 = rgbeToFloat(c01);

    const uint8_t* c11 = &hdrData[(y1 * hdrWidth + x1) * 4];
    XMFLOAT3 col11 = rgbeToFloat(c11);

    col00 = Mult(col00, (1 - dx) * (1 - dy));
    col10 = Mult(col10, dx * (1 - dy));
    col01 = Mult(col01, dy * (1 - dx));
    col11 = Mult(col11, dx * dy);

    return Add(Add(col00, col10), Add(col01, col11));
}

void ReadRLEData(ifstream& fin, uint8_t*& data, int width, int height) 
{
    if (width < 8 || width > 0x7fff)
    {
        fin.read(reinterpret_cast<char*>(data), width * height * 4);
        return;
    }

    uint8_t* scanline_buffer = new uint8_t[width * 4];

    for (int y = 0; y < height; y++)
    {
        uint8_t rgbe[4];
        fin.read(reinterpret_cast<char*>(rgbe), 4);

        if (rgbe[0] != 2 || rgbe[1] != 2 || (rgbe[2] & 0x80))
        {
            // This file is not run length encoded
            data[(y * width + 0) * 4 + 0] = rgbe[0];
            data[(y * width + 0) * 4 + 1] = rgbe[1];
            data[(y * width + 0) * 4 + 2] = rgbe[2];
            data[(y * width + 0) * 4 + 3] = rgbe[3];
            fin.read(reinterpret_cast<char*>(&data[(y * width + 1) * 4]), (width - 1) * 4);
            continue;
        }

        if (((rgbe[2] << 8) | rgbe[3]) != width)
            throw std::exception("Invalid scanline width");

        for (int i = 0; i < 4; i++)
        {
            int ptr = 0;
            while (ptr < width)
            {
                uint8_t buf[2];
                fin.read(reinterpret_cast<char*>(buf), 2);
                if (buf[0] > 128)
                {
                    int count = buf[0] - 128;
                    if ((count == 0) || (count > width - ptr))
                        throw std::exception("Bad RLE data");

                    while (count > 0)
                    {
                        scanline_buffer[ptr] = buf[1];
                        ptr++;
                        count--;
                    }
                    continue;
                }
                
                int count = buf[0];
                if ((count == 0) || (count > width - ptr))
                    throw std::exception("Bad RLE data");

                scanline_buffer[ptr++] = buf[1];
                count--;
                if (count > 0)
                {
                    fin.read(reinterpret_cast<char*>(&scanline_buffer[ptr]), count);
                    ptr += count;
                }
            }

            for (int j = 0; j < width; j++)
            {
                data[(y * width + j) * 4 + i] = scanline_buffer[j];
            }
        }
    }
    delete[] scanline_buffer;
}

void TextureLoader::LoadHDR(string filePath, int& width, int& height, vector<uint8_t*>& pDatas, int& channels)
{
    size_t dotIndex = filePath.find_last_of('.');
    if (dotIndex == std::string::npos)
        throw new std::exception("Invalid file path");    

    string binTexPath0 = filePath.substr(0, dotIndex) + "__0.binTex";
    string binTexPath1 = filePath.substr(0, dotIndex) + "__1.binTex";
    string binTexPath2 = filePath.substr(0, dotIndex) + "__2.binTex";
    string binTexPath3 = filePath.substr(0, dotIndex) + "__3.binTex";
    string binTexPath4 = filePath.substr(0, dotIndex) + "__4.binTex";
    string binTexPath5 = filePath.substr(0, dotIndex) + "__5.binTex";

    if (std::filesystem::exists(binTexPath0))
    {
        bool hasAlpha = false;

        LoadBinTex(binTexPath0, width, height, &pDatas[0], hasAlpha, channels);
        LoadBinTex(binTexPath1, width, height, &pDatas[1], hasAlpha, channels);
        LoadBinTex(binTexPath2, width, height, &pDatas[2], hasAlpha, channels);
        LoadBinTex(binTexPath3, width, height, &pDatas[3], hasAlpha, channels);
        LoadBinTex(binTexPath4, width, height, &pDatas[4], hasAlpha, channels);
        LoadBinTex(binTexPath5, width, height, &pDatas[5], hasAlpha, channels);
        return;
    }

    ifstream fin;
    fin.open(filePath, std::ios::binary);

    if (!fin)
        throw std::exception("IO Exception");

    string confirmation;
    std::getline(fin, confirmation);
    if (confirmation != "#?RADIANCE")
        throw std::exception("Invalid HDR file");

    string madeWithPhotoshop;
    std::getline(fin, madeWithPhotoshop);
    if (madeWithPhotoshop != "# Made with Adobe Photoshop")
        throw std::exception("Invalid HDR file");

    string gammaStr;
    std::getline(fin, gammaStr);
    size_t equalsIndex = gammaStr.find_first_of('=');
    int gamma = atoi(gammaStr.substr(equalsIndex + 1, gammaStr.size() - equalsIndex - 1).c_str());

    string primariesStr;
    std::getline(fin, primariesStr);

    string formatStr;
    std::getline(fin, formatStr);
    if (formatStr != "FORMAT=32-bit_rle_rgbe")
        throw std::exception("Invalid HDR file");

    fin.get();

    string sizeStr;
    std::getline(fin, sizeStr);
    size_t Yindex = sizeStr.find_first_of('Y');
    size_t Xindex = sizeStr.find_first_of('X');
    int hdrHeight = atoi(sizeStr.substr(Yindex + 2, Xindex - 2 - Yindex + 2).c_str());
    int hdrWidth = atoi(sizeStr.substr(Xindex + 2, sizeStr.size() - Xindex + 1).c_str());

    channels = 4;
    int totalPixelCount = hdrWidth * hdrHeight;
    int totalChannelCount = totalPixelCount * channels;
    uint8_t* hdrData = new uint8_t[totalChannelCount];
    ReadRLEData(fin, hdrData, hdrWidth, hdrHeight);

    vector<XMFLOAT3> faceDirections = {
        {-1, 0, 0},
        {1, 0, 0},         
        {0, 1, 0}, 
        {0, -1, 0},
        {0, 0, -1},
        {0, 0, 1}        
    };

    vector<XMFLOAT3> faceRights = {
        {0, 0, 1},
        {0, 0, -1},
        {-1, 0, 0},
        {-1, 0, 0},
        {-1, 0, 0},
        {1, 0, 0}
    };

    vector<XMFLOAT3> faceUps = {
        {0, -1, 0}, 
        {0, -1, 0},
        {0, 0, -1}, 
        {0, 0, 1}, 
        {0, -1, 0}, 
        {0, -1, 0}
    };

    pDatas.resize(6);

    height = hdrHeight;
    width = hdrHeight;

    for (int face = 0; face < 6; ++face)
    {
        pDatas[face] = new uint8_t[width * height * channels];

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                float u = ((x / static_cast<float>(width - 1)) - 0.5f) * 2;
                float v = ((y / static_cast<float>(height - 1)) - 0.5f) * 2;

                XMFLOAT3 up = Mult(faceUps[face], v);
                XMFLOAT3 right = Mult(faceRights[face], u);
                XMFLOAT3 direction = Add(Add(faceDirections[face], up), right);
                direction = Normalize(direction);

                XMFLOAT3 col = SampleHDR(hdrData, direction, hdrWidth, hdrHeight);
                col = Saturate(col);

                int texIndex = y * width + x;
                pDatas[face][texIndex * 4 + 0] = static_cast<uint8_t>(col.x * 255);
                pDatas[face][texIndex * 4 + 1] = static_cast<uint8_t>(col.y * 255);
                pDatas[face][texIndex * 4 + 2] = static_cast<uint8_t>(col.z * 255);
                pDatas[face][texIndex * 4 + 3] = 255u;
            }
    }

    fin.close();
    delete[] hdrData;   

    StoreBinTex(binTexPath0, width, height, pDatas[0], false, channels, false, false);
    StoreBinTex(binTexPath1, width, height, pDatas[1], false, channels, false, false);
    StoreBinTex(binTexPath2, width, height, pDatas[2], false, channels, false, false);
    StoreBinTex(binTexPath3, width, height, pDatas[3], false, channels, false, false);
    StoreBinTex(binTexPath4, width, height, pDatas[4], false, channels, false, false);
    StoreBinTex(binTexPath5, width, height, pDatas[5], false, channels, false, false);
}

void TextureLoader::CreateMipMaps(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, ID3D12Resource* pResource, D3D12_RESOURCE_DESC texDesc)
{
    auto device = d3d->GetDevice();

    if (texDesc.MipLevels <= 1 || texDesc.DepthOrArraySize != 1)
        return;

    std::unique_lock<std::mutex> lock(ms_mutexMipMapGen);

    if (!ms_mipMapRootSig)
        throw std::exception();

    D3D12_SHADER_RESOURCE_VIEW_DESC srcSRVDesc = {};
    srcSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srcSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srcSRVDesc.Format = texDesc.Format;

    D3D12_UNORDERED_ACCESS_VIEW_DESC dstUAVDesc = {};
    dstUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    dstUAVDesc.Format = texDesc.Format;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2 * (texDesc.MipLevels - 1);
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap* heapForCS;
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heapForCS));
    ThrowIfFailed(hr);
    ms_trackedDescHeaps.push_back(heapForCS);

    cmdListDirect->SetComputeRootSignature(ms_mipMapRootSig);
    cmdListDirect->SetDescriptorHeaps(1, &heapForCS);
    cmdListDirect->SetPipelineState(ms_mipMapPSO);

    auto cpuHandle = heapForCS->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = heapForCS->GetGPUDescriptorHandleForHeapStart();

    int width = static_cast<int>(texDesc.Width);
    int height = static_cast<int>(texDesc.Height);

    for (int mip = 0; mip < texDesc.MipLevels - 1; mip++)
    {
        int dstWidth = std::max(width >> (mip + 1), 1);
        int dstHeight = std::max(height >> (mip + 1), 1);

        {
            float texelWidth = 1.0f / float(dstWidth);
            float texelHeight = 1.0f / float(dstHeight);

            cmdListDirect->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&texelWidth), 0);
            cmdListDirect->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&texelHeight), 1);
            if (SettingsManager::ms_Dynamic.MipMapDebugMode)
                cmdListDirect->SetComputeRoot32BitConstant(0, mip, 2);
        }

        srcSRVDesc.Texture2D.MipLevels = 1;
        srcSRVDesc.Texture2D.MostDetailedMip = mip;

        dstUAVDesc.Texture2D.MipSlice = mip + 1;

        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleSrc(cpuHandle, mip * 2, ms_descriptorSize);
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleSrc(gpuHandle, mip * 2, ms_descriptorSize);

        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleDst(cpuHandle, mip * 2 + 1, ms_descriptorSize);
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleDst(gpuHandle, mip * 2 + 1, ms_descriptorSize);

        device->CreateShaderResourceView(pResource, &srcSRVDesc, cpuHandleSrc);
        device->CreateUnorderedAccessView(pResource, nullptr, &dstUAVDesc, cpuHandleDst);

        cmdListDirect->SetComputeRootDescriptorTable(1, gpuHandleSrc);
        cmdListDirect->SetComputeRootDescriptorTable(2, gpuHandleDst);

        cmdListDirect->Dispatch(std::max(dstWidth / 8, 1), std::max(dstHeight / 8, 1), 1);

        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(pResource);
        cmdListDirect->ResourceBarrier(1, &uavBarrier);
    }
}

void TextureLoader::CreateMipMapsCubemap(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, ID3D12Resource* pResource, D3D12_RESOURCE_DESC texDesc)
{
    auto device = d3d->GetDevice();

    if (texDesc.MipLevels <= 1 || texDesc.DepthOrArraySize != 6)
        return;

    if (!ms_mipMapRootSig || !ms_mipMapPSOCubemap)
        throw std::exception();

    D3D12_SHADER_RESOURCE_VIEW_DESC srcSRVDesc = {};
    srcSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srcSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srcSRVDesc.Format = texDesc.Format;

    srcSRVDesc.TextureCube.MipLevels = 1;
    srcSRVDesc.TextureCube.MostDetailedMip = 0;

    D3D12_UNORDERED_ACCESS_VIEW_DESC dstUAVDesc = {};
    dstUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    dstUAVDesc.Format = texDesc.Format;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2 * (texDesc.MipLevels - 1) * 6;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap* heapForCS;
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heapForCS));
    ThrowIfFailed(hr);
    ms_trackedDescHeaps.push_back(heapForCS);

    cmdListDirect->SetComputeRootSignature(ms_mipMapRootSigCubemap);
    cmdListDirect->SetDescriptorHeaps(1, &heapForCS);
    cmdListDirect->SetPipelineState(ms_mipMapPSOCubemap);

    auto cpuHandle = heapForCS->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = heapForCS->GetGPUDescriptorHandleForHeapStart();

    int width = static_cast<int>(texDesc.Width);
    int height = static_cast<int>(texDesc.Height);

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleSrc(cpuHandle, 0, ms_descriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleSrc(gpuHandle, 0, ms_descriptorSize);

    device->CreateShaderResourceView(pResource, &srcSRVDesc, cpuHandleSrc);
    cmdListDirect->SetComputeRootDescriptorTable(1, gpuHandleSrc);

    for (int arraySlice = 0; arraySlice < 6; arraySlice++)
    {
        for (int mip = 0; mip < texDesc.MipLevels - 1; mip++)
        {
            int dstWidth = std::max(width >> (mip + 1), 1);
            int dstHeight = std::max(height >> (mip + 1), 1);

            {
                float texelWidth = 1.0f / float(dstWidth);
                float texelHeight = 1.0f / float(dstHeight);
                float roughness = mip / float(texDesc.MipLevels - 2);

                cmdListDirect->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&texelWidth), 0);
                cmdListDirect->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&texelHeight), 1);
                cmdListDirect->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&roughness), 2);
                cmdListDirect->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&arraySlice), 3);
            }

            dstUAVDesc.Texture2DArray.MipSlice = mip + 1;   
            dstUAVDesc.Texture2DArray.FirstArraySlice = arraySlice;
            dstUAVDesc.Texture2DArray.ArraySize = 1;

            UINT dstSubresourceIndex = D3D12CalcSubresource(mip + 1, arraySlice, 0, texDesc.MipLevels, 6);                       

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleDst(cpuHandle, dstSubresourceIndex, ms_descriptorSize);
            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleDst(gpuHandle, dstSubresourceIndex, ms_descriptorSize);
            
            device->CreateUnorderedAccessView(pResource, nullptr, &dstUAVDesc, cpuHandleDst);
            
            cmdListDirect->SetComputeRootDescriptorTable(2, gpuHandleDst);

            cmdListDirect->Dispatch(std::max(dstWidth / 8, 1), std::max(dstHeight / 8, 1), 1);

            auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(pResource);
            cmdListDirect->ResourceBarrier(1, &uavBarrier);
        }
    }
}

void TextureLoader::CreateIrradianceMap(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, ID3D12Resource* srcResource, ID3D12Resource* dstResource)
{
    auto device = d3d->GetDevice();

    if (!ms_irradianceRootSig)
        throw std::exception();

    D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSRVDesc = {};
    srcTextureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srcTextureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srcTextureSRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srcTextureSRVDesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUAVDesc = {};
    destTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    destTextureUAVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    destTextureUAVDesc.Texture2DArray.ArraySize = 6;
    destTextureUAVDesc.Texture2DArray.FirstArraySlice = 0;
    destTextureUAVDesc.Texture2DArray.MipSlice = 0;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap* descriptorHeap;
    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
    ms_trackedDescHeaps.push_back(descriptorHeap);

    cmdListDirect->SetComputeRootSignature(ms_irradianceRootSig);
    cmdListDirect->SetPipelineState(ms_irradiancePSO);
    cmdListDirect->SetDescriptorHeaps(1, &descriptorHeap);

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentCPUHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, ms_descriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE currentGPUHandle(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, ms_descriptorSize);

    device->CreateShaderResourceView(srcResource, &srcTextureSRVDesc, currentCPUHandle);
    currentCPUHandle.Offset(1, ms_descriptorSize);
    device->CreateUnorderedAccessView(dstResource, nullptr, &destTextureUAVDesc, currentCPUHandle);

    cmdListDirect->SetComputeRootDescriptorTable(0, currentGPUHandle);
    currentGPUHandle.Offset(1, ms_descriptorSize);
    cmdListDirect->SetComputeRootDescriptorTable(1, currentGPUHandle);
    currentGPUHandle.Offset(1, ms_descriptorSize);

    int threads = SettingsManager::ms_Misc.IrradianceMapResolution / 8;

    cmdListDirect->Dispatch(threads, threads, 1);

    //ResourceManager::TransitionResource(cmdListDirect, dstResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SettingsManager::ms_DX12.SRVFormat);
}

void TextureLoader::InitComputeShaders(ID3D12Device2* device)
{
    InitMipMapCS(device);
    InitMipMapPSOCubemap(device);
    InitIrradianceCS(device);
}

void TextureLoader::Shutdown()
{
    if (ms_mipMapRootSig)
    {
        ms_mipMapRootSig->Release();
        ms_mipMapRootSig = 0;
    }

    if (ms_mipMapPSO)
    {
        ms_mipMapPSO->Release();
        ms_mipMapPSO = 0;
    }
   
    if (ms_mipMapPSOCubemap)
    {
        ms_mipMapPSOCubemap->Release();
        ms_mipMapPSOCubemap = 0;
    }

    if (ms_mipMapRootSigCubemap)
    {
        ms_mipMapRootSigCubemap->Release();
        ms_mipMapRootSigCubemap = 0;
    }

    for (int i = 0; i < ms_trackedDescHeaps.size(); i++)
    {
        if (ms_trackedDescHeaps.at(i))
            ms_trackedDescHeaps.at(i)->Release();
    }
    ms_trackedDescHeaps.clear();
}

void TextureLoader::InitMipMapCS(ID3D12Device2* device)
{
    ms_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //The compute shader expects 2 floats, the source texture and the destination texture
    CD3DX12_DESCRIPTOR_RANGE srvCbvRanges[2];
    CD3DX12_ROOT_PARAMETER rootParameters[3];
    srvCbvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    srvCbvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

    int constantsCount = SettingsManager::ms_Dynamic.MipMapDebugMode ? 3 : 2;
    rootParameters[0].InitAsConstants(constantsCount, 0);
    rootParameters[1].InitAsDescriptorTable(1, &srvCbvRanges[0]);
    rootParameters[2].InitAsDescriptorTable(1, &srvCbvRanges[1]);

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

    ID3DBlob* signature;
    ID3DBlob* error;
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&ms_mipMapRootSig));

    ComPtr<ID3DBlob> cBlob;
    wstring path = Application::GetEXEDirectoryPath() + (SettingsManager::ms_Dynamic.MipMapDebugMode ? L"/CreateMipMapsDebug.cso" : L"/CreateMipMaps.cso");
    HRESULT hr = D3DReadFileToBlob(path.c_str(), &cBlob);
    ThrowIfFailed(hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ms_mipMapRootSig;
    psoDesc.CS = { reinterpret_cast<UINT8*>(cBlob->GetBufferPointer()), cBlob->GetBufferSize() };

    device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ms_mipMapPSO));
}

void TextureLoader::InitMipMapPSOCubemap(ID3D12Device2* device)
{
    CD3DX12_DESCRIPTOR_RANGE srvCbvRanges[2];
    CD3DX12_ROOT_PARAMETER rootParameters[3];
    srvCbvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    srvCbvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

    int constantsCount = 4;
    rootParameters[0].InitAsConstants(constantsCount, 0);
    rootParameters[1].InitAsDescriptorTable(1, &srvCbvRanges[0]);
    rootParameters[2].InitAsDescriptorTable(1, &srvCbvRanges[1]);

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

    ID3DBlob* signature;
    ID3DBlob* error;
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&ms_mipMapRootSigCubemap));

    ComPtr<ID3DBlob> cBlob;
    wstring path = Application::GetEXEDirectoryPath() +  L"/CreateMipMapsCubemap.cso";
    HRESULT hr = D3DReadFileToBlob(path.c_str(), &cBlob);
    ThrowIfFailed(hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ms_mipMapRootSigCubemap;
    psoDesc.CS = { reinterpret_cast<UINT8*>(cBlob->GetBufferPointer()), cBlob->GetBufferSize() };

    device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ms_mipMapPSOCubemap));
}

void TextureLoader::InitIrradianceCS(ID3D12Device2* device)
{
    ms_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //The compute shader expects 2 floats, the source texture and the destination texture
    CD3DX12_DESCRIPTOR_RANGE srvCbvRanges[2];
    CD3DX12_ROOT_PARAMETER rootParameters[2];
    srvCbvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    srvCbvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

    rootParameters[0].InitAsDescriptorTable(1, &srvCbvRanges[0]);
    rootParameters[1].InitAsDescriptorTable(1, &srvCbvRanges[1]);

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

    ID3DBlob* signature;
    ID3DBlob* error;
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&ms_irradianceRootSig));

    ComPtr<ID3DBlob> cBlob;
    wstring path = Application::GetEXEDirectoryPath() + L"/IrradianceMap.cso";
    HRESULT hr = D3DReadFileToBlob(path.c_str(), &cBlob);
    ThrowIfFailed(hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ms_irradianceRootSig;
    psoDesc.CS = { reinterpret_cast<UINT8*>(cBlob->GetBufferPointer()), cBlob->GetBufferSize() };

    device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ms_irradiancePSO));
}

bool TextureLoader::ManuallyDetermineHasAlpha(size_t bytes, int channels, uint8_t* pData)
{
    if (channels != 4)
        return false;

    for (size_t i = channels - 1; i < bytes; i += channels)
    {
        uint8_t a = pData[i];
        if (a < UINT8_MAX)
            return true;
    }
    return false;
}
