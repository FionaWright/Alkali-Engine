#include "pch.h"
#include "ModelLoaderObj.h"
#include "Utils.h"
#include <direct.h>
#include <filesystem>

#include "ResourceTracker.h"

#include "AssetFactory.h"
#include <AlkaliGUIManager.h>

namespace filesystem = std::filesystem;

vector<XMFLOAT3> ModelLoaderObj::ms_posList;
vector<XMFLOAT2> ModelLoaderObj::ms_texList;
vector<XMFLOAT3> ModelLoaderObj::ms_norList;
vector<ObjObject> ModelLoaderObj::ms_indexList;

constexpr bool RIGHT_HANDED_TO_LEFT = true;

void ModelLoaderObj::PreprocessObjFile(string filePath, bool split, bool invert)
{
	size_t lastSlashPos = filePath.find_last_of("\\/");
	size_t lastDotPos = filePath.find_last_of(".");
	string folderName = (lastSlashPos != std::string::npos) ? filePath.substr(lastSlashPos + 1, lastDotPos - lastSlashPos - 1) : filePath;
	string folderPath = "Assets/Models/" + folderName;

	struct _stat64i32 info;
	bool dirExists = (_stat(folderPath.c_str(), &info) == 0 && info.st_mode & _S_IFDIR);

	if (split && !dirExists)
	{
		int result = _mkdir(folderPath.c_str());
		if (result != 0)
			throw new std::exception("IO Exception (OUTPUT)");
	}

	ifstream fin;
	char input = '0';

	fin.open(filePath);

	if (fin.fail())
		throw new std::exception("IO Exception");

	ms_posList.clear();
	ms_texList.clear();
	ms_norList.clear();
	ms_indexList.clear();

	int objectIndex = -1;

	if (!split)
	{
		objectIndex = 0;
		ObjObject object;
		object.Name = folderPath;
		ms_indexList.push_back(object);
	}

	while (fin.get(input))
	{
		if (input == 'f')
		{
			if (fin.peek() != ' ')
				continue;

			ObjFaceVertexIndices indices;

			for (int i = 0; i < 3; i++)
			{
				indices = {};

				fin >> indices.PositionIndex;
				indices.PositionIndex--;

				if (fin.peek() == '/')
					fin.get();

				if (fin.peek() == '/')
					throw new std::exception("Invalid obj format. Make sure to triangulate");

				fin >> indices.TextureIndex;
				indices.TextureIndex--;

				if (fin.peek() == '/')
					fin.get();

				fin >> indices.NormalIndex;
				indices.NormalIndex--;

				ms_indexList.at(objectIndex).IndexList.push_back(indices);
			}

			if (fin.peek() == ' ')
			{
				auto firstIndices = ms_indexList.at(objectIndex).IndexList.at(ms_indexList.at(objectIndex).IndexList.size() - 3);
				ms_indexList.at(objectIndex).IndexList.push_back(firstIndices);
				ms_indexList.at(objectIndex).IndexList.push_back(indices);

				fin >> indices.PositionIndex;
				indices.PositionIndex--;

				if (fin.peek() == '/')
					fin.get();

				if (fin.peek() == '/')
					throw new std::exception("Invalid obj format. Make sure to triangulate");

				fin >> indices.TextureIndex;
				indices.TextureIndex--;

				if (fin.peek() == '/')
					fin.get();

				fin >> indices.NormalIndex;
				indices.NormalIndex--;

				ms_indexList.at(objectIndex).IndexList.push_back(indices);
			}

			continue;
		}

		if (split && input == 'u' && NextCharactersMatch(fin, "semtl", false))
		{
			if (fin.peek() != ' ')
				continue;

			ObjObject object;

			fin.get(); // Remove ' '
			fin >> object.Name;

			bool foundExistingObject = false;
			for (size_t i = 0; i < ms_indexList.size(); i++)
			{
				if (ms_indexList.at(i).Name == object.Name)
				{
					objectIndex = static_cast<int>(i);
					foundExistingObject = true;
					break;
				}
			}
			if (foundExistingObject)
				continue;

			ms_indexList.push_back(object);
			objectIndex = static_cast<int>(ms_indexList.size() - 1);
			continue;
		}

		if (input != 'v')
			continue;

		fin.get(input);

		if (input == 't')
		{
			if (fin.peek() != ' ')
				continue;

			XMFLOAT2 tex;
			fin >> tex.x;
			fin >> tex.y;
			ms_texList.push_back(tex);
			continue;
		}

		if (input != ' ' && input != 'n')
			continue;

		if (input == 'n' && fin.peek() != ' ')
			continue;

		XMFLOAT3 vec;
		fin >> vec.x;
		fin >> vec.y;
		fin >> vec.z;

		if (RIGHT_HANDED_TO_LEFT)
			vec.x = -vec.x;

		if (input == 'n')
		{
			ms_norList.push_back(vec);
		}
		else
			ms_posList.push_back(vec);
	}

	fin.close();

	if (!split)
	{
		string outputPath = folderPath + (invert ? " (Inverted)" : "") + ".model";

		SaveObject(outputPath, ms_indexList.at(0).IndexList, invert);

		ms_posList.clear();
		ms_texList.clear();
		ms_norList.clear();
		return;
	}

	for (size_t i = 0; i < ms_indexList.size(); i++)
	{
		if (ms_indexList.at(i).IndexList.size() == 0)
			continue;

		string outputPath = folderPath + "/" + ms_indexList.at(i).Name + (invert ? " (Inverted)" : "") + ".model";

		SaveObject(outputPath, ms_indexList.at(i).IndexList, invert);
	}

	ms_posList.clear();
	ms_texList.clear();
	ms_norList.clear();
}

void ModelLoaderObj::SaveObject(string outputPath, vector<ObjFaceVertexIndices>& objIndices, bool invert)
{
	size_t vertexCount = objIndices.size();

	vector<VertexInputData> vertexBuffer;
	unordered_map<VertexKey, int32_t> vertexMap;
	vector<int32_t> indexBuffer;

	XMFLOAT3 rollingCentroidSum = XMFLOAT3(0, 0, 0);

	bool invertOrder = XOR(invert, RIGHT_HANDED_TO_LEFT);

	for (int32_t i = 0; i < vertexCount - 2; i += 3)
	{
		int32_t i1 = i + 1;
		int32_t i2 = i + 2;

		XMFLOAT2 t0 = ms_texList.at(objIndices.at(i).TextureIndex);
		XMFLOAT2 t1 = ms_texList.at(objIndices.at(i1).TextureIndex);
		XMFLOAT2 t2 = ms_texList.at(objIndices.at(i2).TextureIndex);		

		if (invertOrder)
		{
			TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i2, i1, i, rollingCentroidSum);
			TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i1, i, i2, rollingCentroidSum);
			TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i, i1, i2, rollingCentroidSum);
			continue;
		}

		TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i, i1, i2, rollingCentroidSum);
		TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i1, i, i2, rollingCentroidSum);
		TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i2, i1, i, rollingCentroidSum);
	}

	rollingCentroidSum = Divide(rollingCentroidSum, static_cast<float>(vertexBuffer.size()));
	if (vertexBuffer.size() == 0)
		rollingCentroidSum = XMFLOAT3_ZERO;

	float boundingRadiusSq = 0;

	for (size_t i = 0; i < vertexBuffer.size(); i++)
	{
		XMFLOAT3 pos = vertexBuffer.at(i).Position;
		XMFLOAT3 diff = Subtract(rollingCentroidSum, pos);
		float magSq = Dot(diff, diff);
		if (magSq > boundingRadiusSq)
			boundingRadiusSq = magSq;
	}

	boundingRadiusSq = std::sqrt(boundingRadiusSq);

	ofstream fout;
	std::ios_base::openmode openFlags = std::ios::binary | std::ios::out | std::ios::trunc;
	fout.open(outputPath, openFlags);
	if (!fout)
		throw new std::exception("IO Exception (OUTPUT)");

	fout.write(reinterpret_cast<const char*>(&boundingRadiusSq), sizeof(float));
	fout.write(reinterpret_cast<const char*>(&rollingCentroidSum), sizeof(XMFLOAT3));

	size_t vertexCountUINT = vertexBuffer.size();
	fout.write(reinterpret_cast<const char*>(&vertexCountUINT), sizeof(size_t));

	for (int i = 0; i < vertexBuffer.size(); i++)
		fout.write(reinterpret_cast<const char*>(&vertexBuffer.at(i)), sizeof(VertexInputData));

	size_t indexCountUINT = indexBuffer.size();
	fout.write(reinterpret_cast<const char*>(&indexCountUINT), sizeof(size_t));

	for (int i = 0; i < indexBuffer.size(); i++)
		fout.write(reinterpret_cast<const char*>(&indexBuffer.at(i)), sizeof(int32_t));

	fout.close();

	if (!fout.good())
		throw new std::exception("IO Exception (OUTPUT)");
}

void ModelLoaderObj::TryAddVertex(vector<VertexInputData>& vertexBuffer, vector<int32_t>& indexBuffer, vector<ObjFaceVertexIndices>& objIndices, unordered_map<VertexKey, int32_t>& vertexMap, int32_t i, int32_t i1, int32_t i2, XMFLOAT3& rollingCentroidSum)
{
	auto indices = objIndices.at(i);
	VertexKey key = { indices.PositionIndex, indices.NormalIndex };

	if (!vertexMap.contains(key))
	{
		auto indices1 = objIndices.at(i1);
		auto indices2 = objIndices.at(i2);

		VertexInputData data = SetVertexData(indices, indices1, indices2);
		vertexBuffer.push_back(data);

		int vBufferIndex = static_cast<int>(vertexBuffer.size() - 1);
		indexBuffer.push_back(vBufferIndex);
		vertexMap.emplace(key, vBufferIndex);

		rollingCentroidSum = Add(rollingCentroidSum, ms_posList.at(indices.PositionIndex));
	}
	else
	{
		int vIndex = vertexMap.at(key);
		indexBuffer.push_back(vIndex);
	}
}

VertexInputData ModelLoaderObj::SetVertexData(ObjFaceVertexIndices i, ObjFaceVertexIndices i1, ObjFaceVertexIndices i2)
{
	VertexInputData vertex;

	vertex.Position = ms_posList.at(i.PositionIndex);
	vertex.Texture = ms_texList.at(i.TextureIndex);

	XMFLOAT3 normal = ms_norList.at(i.NormalIndex);
	vertex.Normal = normal;

	XMFLOAT3 pos1 = ms_posList.at(i1.PositionIndex);
	XMFLOAT3 pos2 = ms_posList.at(i2.PositionIndex);

	XMFLOAT2 tex1 = ms_texList.at(i1.TextureIndex);
	XMFLOAT2 tex2 = ms_texList.at(i2.TextureIndex);

	XMFLOAT3 posVec1 = Subtract(pos1, vertex.Position);
	XMFLOAT3 posVec2 = Subtract(pos2, vertex.Position);

	XMFLOAT2 texVec1 = Subtract(tex1, vertex.Texture);
	XMFLOAT2 texVec2 = Subtract(tex2, vertex.Texture);

	float den = 1.0f / (texVec1.x * texVec2.y - texVec1.y * texVec2.x);

	XMFLOAT3 tangent;

	tangent.x = (texVec2.y * posVec1.x - texVec1.y * posVec2.x) * den;
	tangent.y = (texVec2.y * posVec1.y - texVec1.y * posVec2.y) * den;
	tangent.z = (texVec2.y * posVec1.z - texVec1.y * posVec2.z) * den;

	XMFLOAT3 binormal;

	binormal.x = (texVec1.x * posVec2.x - texVec2.x * posVec1.x) * den;
	binormal.y = (texVec1.x * posVec2.y - texVec2.x * posVec1.y) * den;
	binormal.z = (texVec1.x * posVec2.z - texVec2.x * posVec1.z) * den;

	vertex.Tangent = Normalize(tangent);
	vertex.Binormal = Normalize(binormal);

	return vertex;
}

bool ModelLoaderObj::LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<int32_t>& outIndexBuffer, float& boundingSphereRadius, XMFLOAT3& centroid)
{
	ifstream fin;
	wstring longPath = L"Assets/Models/" + filePath;
	fin.open(longPath, std::ios::binary);

	size_t vertexCount, indexCount;

	if (!fin)
		return false;

	fin.read(reinterpret_cast<char*>(&boundingSphereRadius), sizeof(float));
	fin.read(reinterpret_cast<char*>(&centroid), sizeof(XMFLOAT3));

	fin.read(reinterpret_cast<char*>(&vertexCount), sizeof(size_t));

	outVertexBuffer.resize(vertexCount);
	fin.read(reinterpret_cast<char*>(outVertexBuffer.data()), sizeof(VertexInputData) * vertexCount);

	fin.read(reinterpret_cast<char*>(&indexCount), sizeof(size_t));

	outIndexBuffer.resize(indexCount);
	fin.read(reinterpret_cast<char*>(outIndexBuffer.data()), sizeof(int32_t) * indexCount);

	return true;
}

void ModelLoaderObj::LoadSplitModel(D3DClass* d3d, RootParamInfo& rpi, ID3D12GraphicsCommandList2* cmdList, string name, Batch* batch, shared_ptr<Shader> shader)
{
	string mtlFilePath = "Assets/Models/" + name + "/" + name + ".mtl";

	ifstream fin;
	fin.open(mtlFilePath);

	if (!fin)
		throw new std::exception("IO Exception");

	string line;

	while (std::getline(fin, line))
	{
		if (!line.starts_with("newmtl"))
			continue;

		string modelName = line.substr(line.find_first_of(' ') + 1);

		float specularExponent = 1;
		float opticalDensity = 0;
		float dissolveFactor = 0;
		int illuminationModel = 3;
		string diffuseName = "";
		string specularName = "";
		string specularExpName = "";
		string reflectionName = "";
		string normalName = "";

		// TODO: Ambient and emmissive
		while (std::getline(fin, line))
		{
			if (line.empty())
				break;

			if (line.starts_with("Ns"))
			{
				specularExponent = std::stof(line.substr(line.find_first_of(' ') + 1));
				continue;
			}

			if (line.starts_with("Ni"))
			{
				opticalDensity = std::stof(line.substr(line.find_first_of(' ') + 1));
				continue;
			}

			if (line.starts_with("d"))
			{
				dissolveFactor = std::stof(line.substr(line.find_first_of(' ') + 1));
				continue;
			}

			if (line.starts_with("illum"))
			{
				illuminationModel = std::atoi(line.substr(line.find_first_of(' ') + 1).c_str());
				continue;
			}

			if (line.starts_with("map_Kd"))
			{
				diffuseName = line.substr(line.find_first_of(' ') + 1);
				continue;
			}

			if (line.starts_with("map_Ks"))
			{
				specularName = line.substr(line.find_first_of(' ') + 1);
				continue;
			}

			if (line.starts_with("map_Bump"))
			{
				normalName = line.substr(line.find_last_of(' ') + 1);
				continue;
			}

			if (line.starts_with("map_Ns"))
			{
				specularExpName = line.substr(line.find_first_of(' ') + 1);
				continue;
			}

			if (line.starts_with("map_refl"))
			{
				reflectionName = line.substr(line.find_last_of(' ') + 1);
				continue;
			}
		}

		shared_ptr<Model> model = std::make_shared<Model>();

		string modelPath = name + "/" + modelName + ".model";
		model->Init(cmdList, modelPath);

		string diffuseTexPath = name + "/" + diffuseName;
		if (diffuseName == "")
			diffuseTexPath = "WhitePOT.tga";

		shared_ptr<Texture> diffuseTex;
		if (!ResourceTracker::TryGetTexture(diffuseTexPath, diffuseTex))
		{
			diffuseTex->Init(d3d, cmdList, diffuseTexPath);
		}

		string normalTexPath = name + "/" + normalName;
		if (normalName == "")
			normalTexPath = "DefaultNormal.tga";

		shared_ptr<Texture> normalTex;
		if (!ResourceTracker::TryGetTexture(normalTexPath, normalTex))
		{
			normalTex->Init(d3d, cmdList, normalTexPath);
		}

		vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
		vector<UINT> cbvSizesFrame = { sizeof(CameraCB), sizeof(DirectionalLightCB) };
		vector<shared_ptr<Texture>> textures = { diffuseTex, normalTex };

		shared_ptr<Material> material = AssetFactory::CreateMaterial();
		material->AddCBVs(d3d, cmdList, cbvSizesDraw, false);
		material->AddCBVs(d3d, cmdList, cbvSizesFrame, true);
		material->AddSRVs(d3d, textures);

		GameObject go(modelName, model, shader, material);
		batch->AddGameObject(go);
	}

	fin.close();
}