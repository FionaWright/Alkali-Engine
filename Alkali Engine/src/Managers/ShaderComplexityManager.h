#pragma once

#include "pch.h"
#include <D3DClass.h>
#include <unordered_map>
#include <Shader.h>

using std::unordered_map;
class Model;

class ShaderComplexityManager
{
public:
	static void Init(Model* model);
	static void CalculateComplexityTable(D3DClass* d3d, int backBufferIndex, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect, const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, const D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle, ID3D12Resource* backBufferResource);
	static float GetCostOfShader(Shader* shader);

private:
	static unordered_map<Shader*, float> ms_complexityTable;
	static Model* ms_model;
	static uint32_t ms_rollingMeanFrameCount;
};

