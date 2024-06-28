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
    float p;
};

struct PerFrameCBuffers
{
    CameraCB Camera;
    DirectionalLightCB DirectionalLight;
};