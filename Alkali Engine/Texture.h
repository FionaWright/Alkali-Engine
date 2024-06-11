#pragma once

#include "pch.h"
#include <string>
#include <iostream>
#include <fstream>
#include "GenerateMipsPSO.h"
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

	void Init(ID3D12Device2* device, ID3D12GraphicsCommandList2* commandListDirect, string filePath);
	void Init(ID3D12Device2* device, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool& hasAlpha);

	void AddToDescriptorHeap(D3DClass* d3d, ID3D12DescriptorHeap* srvHeap, int srvHeapOffset);

	void LoadTGA(string filePath);
	void LoadDDS(string filePath, bool& hasAlpha);
	void LoadDDS_DXT1(ifstream& fin);
	void LoadDDS_DXT5(ifstream& fin);
	void LoadDDS_ATI2(ifstream& fin);
	void LoadPNG(string filePath);
	void LoadJPG(string filePath);

	void GenerateMips(D3DClass* d3d, ID3D12DescriptorHeap* descHeap);

	void CreateMipMaps(D3DClass* d3d);

private:
	int m_textureWidth = -1, m_textureHeight = -1;
	uint8_t* m_textureData;

	ComPtr<ID3D12Resource> m_textureUploadHeap;
	ComPtr<ID3D12Resource> m_textureResource;

	D3D12_GPU_DESCRIPTOR_HANDLE m_srvGPUHandle;
	vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_uavGPUHandles;

	D3D12_RESOURCE_DESC m_textureDesc;

	bool m_is2Channel = false;

	static unique_ptr<GenerateMipsPSO> ms_generateMipsPSO; // NEEDS TO BE RESET
};


