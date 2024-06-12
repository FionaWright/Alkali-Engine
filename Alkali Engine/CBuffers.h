#pragma once

#include "pch.h"

struct MatricesCB
{
	XMMATRIX M;
	XMMATRIX InverseTransposeM;
	XMMATRIX VP;
};

struct MatricesLineCB
{
	XMMATRIX VP;
};

struct alignas(16) GenerateMipsCB
{
    uint32_t SrcMipLevel;           // Texture level of source mip
    uint32_t NumMipLevels;          // Number of OutMips to write: [1-4]
    uint32_t SrcDimension;          // Width and height of the source texture are even or odd.
    uint32_t IsSRGB;                // Must apply gamma correction to sRGB textures.
    XMFLOAT2 TexelSize;    // 1.0 / OutMip1.Dimensions
};