#pragma once

#include "pch.h"
#include "Texture.h"

class TextureLoader
{
public:
	static void LoadTex(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels, bool flipUpsideDown = false, bool isNormalMap = false);

	static void StoreBinTex(string filePath, int width, int height, uint8_t* pData, bool hasAlpha, int channels, bool flipUpsideDown, bool isNormalMap);
	static void LoadBinTex(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels);

	static void LoadTGA(string filePath, int& width, int& height, uint8_t** pData);
	static void LoadDDS(string filePath, int& width, int& height, uint8_t** pData, bool& hasAlpha, int& channels);
	static void LoadDDS_DXT1(ifstream& fin, int& width, int& height, uint8_t** pData);
	static void LoadDDS_DXT5(ifstream& fin, int& width, int& height, uint8_t** pData);
	static void LoadDDS_ATI2(ifstream& fin, int& width, int& height, uint8_t** pData);
	static void LoadPNG(string filePath, int& width, int& height, uint8_t** pData, int& channels);
	static void LoadJPG(string filePath, int& width, int& height, uint8_t** pData, int& channels);
	static void LoadHDR(string filePath, int& width, int& height, vector<uint8_t*>& pDatas, int& channels);

	static void CreateMipMaps(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, ID3D12Resource* pResource, D3D12_RESOURCE_DESC texDesc);
	static void CreateMipMapsCubemap(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, ID3D12Resource* pResource, D3D12_RESOURCE_DESC texDesc);
	static void CreateIrradianceMap(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdListDirect, ID3D12Resource* srcResource, ID3D12Resource* dstResource);

	static void Shutdown();

private:
	static void InitMipMapCS(ID3D12Device2* device);
	static void InitMipMapPSOCubemap(ID3D12Device2* device);
	static void InitIrradianceCS(ID3D12Device2* device);
	static bool ManuallyDetermineHasAlpha(size_t bytes, int channels, uint8_t* pData);

	static ID3D12RootSignature* ms_mipMapRootSig, * ms_mipMapRootSigCubemap, * ms_irradianceRootSig;
	static ID3D12PipelineState* ms_mipMapPSO, *ms_mipMapPSOCubemap, * ms_irradiancePSO;

	static int ms_descriptorSize;
	static vector<ID3D12DescriptorHeap*> ms_trackedDescHeaps;
};

