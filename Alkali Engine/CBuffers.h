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
    XMFLOAT3 LightDirection;
    XMFLOAT3 LightDiffuse;
    float SpecularPower;

    XMFLOAT2 p;
};

struct PerFrameCBuffers
{
    CameraCB Camera;
    DirectionalLightCB DirectionalLight;
};