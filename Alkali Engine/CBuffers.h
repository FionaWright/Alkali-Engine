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

struct PerFrameCBuffers
{
    CameraCB Camera;
    DirectionalLightCB DirectionalLight;
};