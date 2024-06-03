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

struct ObjFaceVertexIndices
{
	int PositionIndex;
	int TextureIndex;
	int NormalIndex;
};

struct ObjObject
{
	string Name;
	vector<ObjFaceVertexIndices> IndexList;
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

struct VertexKey
{
	int PositionIndex;
	int NormalIndex;

	bool operator==(const VertexKey& other) const {
		return PositionIndex == other.PositionIndex && NormalIndex == other.NormalIndex;
	}
};

namespace std
{
	template<>
	struct hash<VertexKey>
	{
		size_t operator()(const VertexKey& key) const {
			// Combine hash values of PositionIndex and NormalIndex
			size_t hashValue = 17;
			hashValue = hashValue * 31 + hash<int>()(key.PositionIndex);
			hashValue = hashValue * 31 + hash<int>()(key.NormalIndex);
			return hashValue;
		}
	};
}

class ModelLoader
{
public:
	static void PreprocessObjFile(string filePath, bool split = true);
	static void SaveObject(string outputPath, vector<ObjFaceVertexIndices>& objIndices);
	static void TryAddVertex(vector<VertexInputData>& vertexBuffer, vector<int>& indexBuffer, vector<ObjFaceVertexIndices>& objIndices, unordered_map<VertexKey, int>& vertexMap, int i, int i1, int i2);
	static VertexInputData SetVertexData(ObjFaceVertexIndices i, ObjFaceVertexIndices i1, ObjFaceVertexIndices i2);	
	static void LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<WORD>& outIndexBuffer, size_t& outVertexCount, size_t& outIndexCount);

private:
	static vector<XMFLOAT3> m_posList;
	static vector<XMFLOAT2> m_texList;
	static vector<XMFLOAT3> m_norList;
	static vector<ObjObject> m_indexList;
};

