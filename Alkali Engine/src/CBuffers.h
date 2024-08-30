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

struct MatricesMV_VCB
{
    XMMATRIX M;
    XMMATRIX V;
};

struct MatricesMV_GCB
{
    XMMATRIX P[4];
};

struct CullFlagsCB
{
    UINT Flags;
};

struct CameraCB
{
    XMFLOAT3 CameraPosition;
    float p;
};

struct DirectionalLightCB
{   
    XMFLOAT3 LightDirection;    
    float SpecularPower;
    
#if PACK_COLORS    
    uint32_t LightColor;
    uint32_t AmbientColor;
    XMFLOAT2 p;
#else
    XMFLOAT3 LightColor;
    float p1;

    XMFLOAT3 AmbientColor;
    float p2;
#endif
};

struct MaterialPropertiesCB
{
    float AlphaCutoff = 0.9f;
    float Metallic = 0.5f;
    float Roughness = 0.5f;
#if PACK_COLORS
    uint32_t BaseColorFactor = 0xFFFFFFFFu;
#else
    float p;
    XMFLOAT3 BaseColorFactor = XMFLOAT3_ONE;     
#endif
};

struct ThinFilmCB
{
    float ThicknessMax = 0.0f;
    float ThicknessMin = 0.0f;
    float n0 = 1, n1 = 1.5f, n2 = 1.25f;

private:
    float Delta = 0.0f;

public:
    BOOL Enabled = FALSE;
    float p;

    void CalculateDelta()
    {
        const float d10 = (n1 > n0) ? 0.0f : static_cast<float>(PI);
        const float d12 = (n1 > n2) ? 0.0f : static_cast<float>(PI);
        Delta = d10 + d12;
    }

    float GetDelta() { return Delta; }
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

    XMFLOAT4 PoissonDisc[POISSON_DISC_COUNT];
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