#pragma once

#include "pch.h"

#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

using std::wstring;
using std::string;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::unordered_map;

struct ObjIndexGroup
{
	int PositionIndex;
	int TextureIndex;
	int NormalIndex;
};

#pragma pack(push, 1)
struct VertexInputData
{
	XMFLOAT3 Position;
	XMFLOAT2 Texture;
	XMFLOAT3 Normal;
	XMFLOAT3 Tangent;
	XMFLOAT3 Binormal;
};
#pragma pack(pop)

class ModelLoader
{
public:
	static void PreprocessObjFile(string filePath);
	static VertexInputData SetVertexData(ObjIndexGroup indices, int otherVertexInFaceIndex1, int otherVertexInFaceIndex2);
	static void LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<WORD>& outIndexBuffer, size_t& outVertexCount, size_t& outIndexCount);

private:
	static vector<XMFLOAT3> m_posList;
	static vector<XMFLOAT2> m_texList;
	static vector<XMFLOAT3> m_norList;
	static vector<ObjIndexGroup> m_indexList;
};

