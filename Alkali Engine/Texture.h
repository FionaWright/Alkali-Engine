#pragma once

#include "pch.h"
#include <string>

using std::string;
using std::vector;

class Texture
{
public:
	Texture();
	~Texture();

	void Init(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList2> commandList, string filePath);

	void AddToDescriptorHeap(ComPtr<ID3D12Device2> device, ComPtr<ID3D12DescriptorHeap> srvHeap, int srvHeapOffset);

	void LoadTGA(string filePath);
	void LoadDDS(string filePath);
	void LoadPNG(string filePath);
	void LoadJPG(string filePath);

private:
	int m_textureWidth = -1, m_textureHeight = -1;
	uint8_t* m_textureData;

	ComPtr<ID3D12Resource> m_textureUploadHeap;
	ComPtr<ID3D12Resource> m_texture;
};

