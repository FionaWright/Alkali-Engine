#include "pch.h"
#include "TextureLoader.h"
#include "Settings.h"
#include "Application.h"
#include "ResourceManager.h"
#include "Scene.h"

#include "spng/spng.h"

#include <string>
#include <filesystem>
using std::wstring;
using std::ofstream;

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

ID3D12RootSignature* TextureLoader::ms_mipMapRootSig;
ID3D12PipelineState* TextureLoader::ms_pso;
int TextureLoader::ms_descriptorSize;
vector<ID3D12DescriptorHeap*> TextureLoader::ms_trackedDescHeaps;

void TextureLoader::LoadTex(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels, bool flipUpsideDown, bool isNormalMap)
{
    size_t dotIndex = filePath.find_last_of('.');
    if (dotIndex == std::string::npos)
        throw new std::exception("Invalid file path");

    string binTexPath = filePath.substr(0, dotIndex) + ".binTex";
    bool binTexAlternativeExists = std::filesystem::exists(binTexPath);

    string fileExtension = filePath.substr(dotIndex + 1, filePath.size() - dotIndex - 1);
    if ((!Scene::IsForceReloadBinTex() && binTexAlternativeExists) || fileExtension == "binTex")
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

    if (isNormalMap && channels == 3 && CONVERT_NORMAL_CHANNELS_3_TO_2)
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

    if (isNormalMap && channels == 2 && CONVERT_NORMAL_CHANNELS_2_TO_3)
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

            (*pData)[dPixelIndex + 0] = static_cast<uint8_t>(targaData[spi + 2]);  // Red.
            (*pData)[dPixelIndex + 1] = static_cast<uint8_t>(targaData[spi + 1]);  // Green.
            (*pData)[dPixelIndex + 2] = static_cast<uint8_t>(targaData[spi + 0]);  // Blue

            if (bytesPerPixel == 4)
                (*pData)[dPixelIndex + 3] = static_cast<uint8_t>(targaData[spi + 3]);  // Alpha
            else
                (*pData)[dPixelIndex + 3] = 255u;  // Alpha

            sPixelIndex += bytesPerPixel;
            dPixelIndex += NUM_CHANNELS;
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

    uint8_t decompressedRed[16];
    uint8_t decompressedGreen[16];

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

void TextureLoader::CreateMipMaps(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, ID3D12Resource* pResource, D3D12_RESOURCE_DESC texDesc)
{
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

    int mipLevels = texDesc.MipLevels;
    auto device = d3d->GetDevice();    

    if (mipLevels <= 1)
        return;        

    if (!ms_mipMapRootSig)
        InitComputeShader(device);

    D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSRVDesc = {};
    srcTextureSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srcTextureSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srcTextureSRVDesc.Format = texDesc.Format;
    srcTextureSRVDesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUAVDesc = {};
    destTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    destTextureUAVDesc.Format = texDesc.Format;

    uint32_t requiredHeapSize = mipLevels - 1;

    //Create the descriptor heap with layout: source texture - destination texture
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2 * requiredHeapSize;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap* descriptorHeap;
    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
    ms_trackedDescHeaps.push_back(descriptorHeap);

    commandListDirect->SetComputeRootSignature(ms_mipMapRootSig);
    commandListDirect->SetPipelineState(ms_pso);
    commandListDirect->SetDescriptorHeaps(1, &descriptorHeap);

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentCPUHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, ms_descriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE currentGPUHandle(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, ms_descriptorSize);

    ResourceManager::TransitionResource(commandListDirect, pResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    int width = static_cast<int>(texDesc.Width);
    int height = static_cast<int>(texDesc.Height);

    for (uint32_t TopMip = 0; TopMip < mipLevels - 1; TopMip++)
    {
        uint32_t dstWidth = std::max(width >> (TopMip + 1), 1);
        uint32_t dstHeight = std::max(height >> (TopMip + 1), 1);

        srcTextureSRVDesc.Texture2D.MostDetailedMip = TopMip;
        device->CreateShaderResourceView(pResource, &srcTextureSRVDesc, currentCPUHandle);
        currentCPUHandle.Offset(1, ms_descriptorSize);

        destTextureUAVDesc.Texture2D.MipSlice = TopMip + 1;
        device->CreateUnorderedAccessView(pResource, nullptr, &destTextureUAVDesc, currentCPUHandle);
        currentCPUHandle.Offset(1, ms_descriptorSize);

        commandListDirect->SetComputeRoot32BitConstant(0, DWParam(1.0f / dstWidth).Uint, 0);
        commandListDirect->SetComputeRoot32BitConstant(0, DWParam(1.0f / dstHeight).Uint, 1);
        if (Scene::IsMipMapDebugMode())
            commandListDirect->SetComputeRoot32BitConstant(0, TopMip, 2);

        commandListDirect->SetComputeRootDescriptorTable(1, currentGPUHandle);
        currentGPUHandle.Offset(1, ms_descriptorSize);
        commandListDirect->SetComputeRootDescriptorTable(2, currentGPUHandle);
        currentGPUHandle.Offset(1, ms_descriptorSize);

        commandListDirect->Dispatch(std::max(dstWidth / 8, 1u), std::max(dstHeight / 8, 1u), 1);

        //Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(pResource);
        commandListDirect->ResourceBarrier(1, &uavBarrier);
    }
}

void TextureLoader::Shutdown()
{
    if (ms_mipMapRootSig)
    {
        ms_mipMapRootSig->Release();
        ms_mipMapRootSig = 0;
    }

    if (ms_pso)
    {
        ms_pso->Release();
        ms_pso = 0;
    }

    for (int i = 0; i < ms_trackedDescHeaps.size(); i++)
    {
        if (ms_trackedDescHeaps.at(i))
            ms_trackedDescHeaps.at(i)->Release();
    }
    ms_trackedDescHeaps.clear();
}

void TextureLoader::InitComputeShader(ID3D12Device2* device)
{
    ms_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //The compute shader expects 2 floats, the source texture and the destination texture
    CD3DX12_DESCRIPTOR_RANGE srvCbvRanges[2];
    CD3DX12_ROOT_PARAMETER rootParameters[3];
    srvCbvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    srvCbvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

    int constantsCount = Scene::IsMipMapDebugMode() ? 3 : 2;
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
    wstring path = Application::GetEXEDirectoryPath() + (Scene::IsMipMapDebugMode() ? L"/CreateMipMapsDebug.cso" : L"/CreateMipMaps.cso");
    HRESULT hr = D3DReadFileToBlob(path.c_str(), &cBlob);
    ThrowIfFailed(hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ms_mipMapRootSig;
    psoDesc.CS = { reinterpret_cast<UINT8*>(cBlob->GetBufferPointer()), cBlob->GetBufferSize() };

    device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ms_pso));
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
