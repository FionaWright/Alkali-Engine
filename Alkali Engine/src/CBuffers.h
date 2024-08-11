#pragma once

#include "pch.h"
#include "Constants.h"
#include "Utils.h"
#include "SettingsManager.h"

struct MatricesCB
{
	XMMATRIX M;
	XMMATRIX InverseTransposeM;
    XMMATRIX V;
    XMMATRIX P;
};

struct MatricesLineCB
{
	XMMATRIX VP;
};

struct CameraCB
{
    XMFLOAT3 CameraPosition;
    float p;
};

struct DirectionalLightCB
{
    XMFLOAT3 AmbientColor;
    float p1;

    XMFLOAT3 LightDirection;
    float p2;

    XMFLOAT3 LightDiffuse;
    float SpecularPower;
};

struct MaterialPropertiesCB
{
    XMFLOAT3 BaseColorFactor = XMFLOAT3_ONE;
    float Roughness = 0.5f;

    float AlphaCutoff = 0.5f;
    float IOR = 1.5f;
    float Dispersion = 0.0f;
    float Metallic = 0.5f;
};

struct ShadowMapCB
{
    float NormalBias;
    float CascadeCount;
    XMFLOAT2 p;

    XMMATRIX ShadowMatrix[MAX_SHADOW_MAP_CASCADES];
};

struct ShadowMapPixelCB
{
    XMFLOAT4 CascadeDistances; // Max 4 cascades

    float Bias;
    float ShadowWidthPercent;
    XMFLOAT2 TexelSize;

    float PCFSampleCount;
    float PCFSampleRange;
    float CascadeCount;
    float p;

    XMFLOAT4 PoissonDisc[16];
};

struct DepthViewCB
{
    XMFLOAT2 Resolution;
    float MinValue;
    float MaxValue;
};

struct EnvMapCB
{
    UINT EnvMapMipLevels;
};

struct PerFrameCBuffers_PBR
{
    CameraCB Camera;
    DirectionalLightCB DirectionalLight;
    ShadowMapCB ShadowMap;
    ShadowMapPixelCB ShadowMapPixel;
    EnvMapCB EnvMap;
};
constexpr std::vector<UINT> PER_FRAME_PBR_SIZES() { 
    return { sizeof(CameraCB), sizeof(DirectionalLightCB), sizeof(ShadowMapCB), sizeof(ShadowMapPixelCB), sizeof(EnvMapCB) };
}