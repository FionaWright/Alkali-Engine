#pragma once

#include "pch.h"
#include <string>
#include <iostream>
#include <fstream>
#include "D3DClass.h"

using std::ifstream;
using std::unique_ptr;
using std::string;
using std::vector;

class Texture
{
public:
	Texture();
	~Texture();	

	void Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool flipUpsideDown = false, bool isNormalMap = false);
	void InitCubeMap(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, vector<string>& filePaths, bool flipUpsideDown = false);
	void InitCubeMapHDR(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool flipUpsideDown = false);
	void InitCubeMapUAV_Empty(D3DClass* d3d);

	void AddToDescriptorHeap(D3DClass* d3d, ID3D12DescriptorHeap* srvHeap, size_t srvHeapOffset);

	bool GetHasAlpha();
	string GetFilePath();
	int GetMipLevels();
	int GetChannels();
	ID3D12Resource* GetResource();

private:
	void MakeTexDesc(UINT16 arraySize);
	void CreateResources(ID3D12Device2* device, int numSubresources);
	void UploadResources(ID3D12GraphicsCommandList2* commandListDirect, uint8_t** pData);

	int m_textureWidth = -1, m_textureHeight = -1;

	ComPtr<ID3D12Resource> m_textureUploadHeap;
	ComPtr<ID3D12Resource> m_textureResource;

	D3D12_RESOURCE_DESC m_textureDesc;

	int m_channels = -1;
	bool m_hasAlpha = false;
	bool m_isCubemap = false;
	int m_numSubresources = 1;

	string m_filePath;
};


