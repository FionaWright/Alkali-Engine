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

	void Init(D3DClass* d3d, ID3D12GraphicsCommandList2* commandListDirect, string filePath, bool flipUpsideDown = false);

	void AddToDescriptorHeap(D3DClass* d3d, ID3D12DescriptorHeap* srvHeap, int srvHeapOffset);

	bool GetHasAlpha();
	string GetFilePath();
	int GetMipLevels();

private:
	int m_textureWidth = -1, m_textureHeight = -1;
	uint8_t* m_textureData;

	ComPtr<ID3D12Resource> m_textureUploadHeap;
	ComPtr<ID3D12Resource> m_textureResource;

	D3D12_RESOURCE_DESC m_textureDesc;

	bool m_is2Channel = false;
	bool m_hasAlpha = false;

	string m_filePath;
};


