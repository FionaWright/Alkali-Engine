#pragma once

#include "pch.h"
#include "Texture.h"

class TextureLoader
{
public:
	static void LoadTGA(string filePath, int& width, int& height, uint8_t** pData, bool& is2Channel);
	static void LoadDDS(string filePath, bool& hasAlpha, int& width, int& height, uint8_t** pData, bool& is2Channel);
	static void LoadDDS_DXT1(ifstream& fin, int& width, int& height, uint8_t** pData);
	static void LoadDDS_DXT5(ifstream& fin, int& width, int& height, uint8_t** pData);
	static void LoadDDS_ATI2(ifstream& fin, int& width, int& height, uint8_t** pData);
	static void LoadPNG(string filePath, int& width, int& height, uint8_t** pData, bool& is2Channel);
	static void LoadJPG(string filePath, int& width, int& height, uint8_t** pData, bool& is2Channel);

	static void CreateMipMaps(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, ID3D12Resource* pResource, D3D12_RESOURCE_DESC texDesc);

	static void Shutdown();

private:
	static void InitComputeShader(ID3D12Device2* device);
	static ID3D12RootSignature* ms_mipMapRootSig;
	static ID3D12PipelineState* ms_pso;
	static int ms_descriptorSize;
	static vector<ID3D12DescriptorHeap*> ms_trackedDescHeaps;
};

