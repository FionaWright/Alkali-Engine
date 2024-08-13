#pragma once

#include "pch.h"

#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include "Batch.h"

#include "fastgltf/include/fastgltf/core.hpp"
#include "fastgltf/include/fastgltf/types.hpp"

using std::wstring;
using std::string;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::unordered_map;

struct ObjFaceVertexIndices
{
	int32_t PositionIndex;
	int32_t TextureIndex;
	int32_t NormalIndex;
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
	int32_t PositionIndex;
	int32_t NormalIndex;

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
			size_t hashValue = 17;
			hashValue = hashValue * 31 + hash<int32_t>()(key.PositionIndex);
			hashValue = hashValue * 31 + hash<int32_t>()(key.NormalIndex);
			return hashValue;
		}
	};
}

class ModelLoader
{
public:
	static void PreprocessObjFile(string filePath, bool split = true, bool invert = false);
	static void SaveObject(string outputPath, vector<ObjFaceVertexIndices>& objIndices, bool invert);
	static void TryAddVertex(vector<VertexInputData>& vertexBuffer, vector<int32_t>& indexBuffer, vector<ObjFaceVertexIndices>& objIndices, unordered_map<VertexKey, int32_t>& vertexMap, int32_t i, int32_t i1, int32_t i2, XMFLOAT3& rollingCentroidSum);
	static VertexInputData SetVertexData(ObjFaceVertexIndices i, ObjFaceVertexIndices i1, ObjFaceVertexIndices i2);	
	static bool LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<int32_t>& outIndexBuffer, float& boundingSphereRadius, XMFLOAT3& centroid);

	static void LoadSplitModel(D3DClass* d3d, RootParamInfo& rpi, ID3D12GraphicsCommandList2* cmdList, string name, Batch* batch, shared_ptr<Shader> shader);

	//static void LoadModelGLTF(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<int32_t>& outIndexBuffer, float& boundingSphereRadius, XMFLOAT3& centroid);
	static void LoadSplitModelGLTF(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, string name, RootParamInfo& rpi, Batch* batch, shared_ptr<Texture> skyboxTex, shared_ptr<Texture> irradianceTex, shared_ptr<Shader> shader, shared_ptr<Shader> shaderCullOff = nullptr, vector<string>* optNameWhiteList = nullptr, Transform defaultTransform = {});

	static vector<shared_ptr<Model>> LoadModelsFromGLTF(D3DClass* d3d, ID3D12GraphicsCommandList2* cmdList, string modelName);

private:
	static vector<XMFLOAT3> ms_posList;
	static vector<XMFLOAT2> ms_texList;
	static vector<XMFLOAT3> ms_norList;
	static vector<ObjObject> ms_indexList;

	static fastgltf::Parser ms_parser;
	static bool ms_initialisedParser;
};

