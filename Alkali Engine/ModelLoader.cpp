#include "pch.h"
#include "ModelLoader.h"
#include "Utils.h"
#include <direct.h>
#include <filesystem>

namespace filesystem = std::filesystem;

vector<XMFLOAT3> ModelLoader::m_posList;
vector<XMFLOAT2> ModelLoader::m_texList;
vector<XMFLOAT3> ModelLoader::m_norList;
vector<ObjObject> ModelLoader::m_indexList;

constexpr bool RIGHT_HANDED_TO_LEFT = true;

void ModelLoader::PreprocessObjFile(string filePath, bool split)
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

	m_posList.clear();
	m_texList.clear();
	m_norList.clear();
	m_indexList.clear();

	int objectIndex = -1;

	if (!split)
	{
		objectIndex = 0;
		ObjObject object;
		object.Name = folderPath;
		m_indexList.push_back(object);
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

				m_indexList.at(objectIndex).IndexList.push_back(indices);
			}

			if (fin.peek() == ' ')
			{
				auto firstIndices = m_indexList.at(objectIndex).IndexList.at(m_indexList.at(objectIndex).IndexList.size() - 3);
				m_indexList.at(objectIndex).IndexList.push_back(firstIndices);
				m_indexList.at(objectIndex).IndexList.push_back(indices);				

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
				
				m_indexList.at(objectIndex).IndexList.push_back(indices);							
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
			for (size_t i = 0; i < m_indexList.size(); i++)
			{
				if (m_indexList.at(i).Name == object.Name)
				{
					objectIndex = i;
					foundExistingObject = true;
					break;
				}
			}
			if (foundExistingObject)
				continue;

			m_indexList.push_back(object);
			objectIndex = m_indexList.size() - 1;

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
			m_texList.push_back(tex);
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
			m_norList.push_back(vec);
		}			
		else
			m_posList.push_back(vec);
	}

	fin.close();

	if (!split)
	{
		string outputPath = folderPath + ".model";

		SaveObject(outputPath, m_indexList.at(0).IndexList);

		m_posList.clear();
		m_texList.clear();
		m_norList.clear();
		return;
	}

	for (size_t i = 0; i < m_indexList.size(); i++)
	{
		if (m_indexList.at(i).IndexList.size() == 0)
			continue;

		string outputPath = folderPath + "/" + m_indexList.at(i).Name + ".model";

		SaveObject(outputPath, m_indexList.at(i).IndexList);
	}

	m_posList.clear();
	m_texList.clear();
	m_norList.clear();
}

void ModelLoader::SaveObject(string outputPath, vector<ObjFaceVertexIndices>& objIndices) 
{
	size_t vertexCount = objIndices.size();

	vector<VertexInputData> vertexBuffer;
	unordered_map<VertexKey, int32_t> vertexMap;
	vector<int32_t> indexBuffer;
	
	XMFLOAT3 rollingCentroidSum = XMFLOAT3(0, 0, 0);

	for (int32_t i = 0; i < vertexCount - 2; i += 3)
	{
		int32_t i1 = i + 1;
		int32_t i2 = i + 2;

		if (RIGHT_HANDED_TO_LEFT)
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

	rollingCentroidSum = Divide(rollingCentroidSum, vertexBuffer.size());

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

void ModelLoader::TryAddVertex(vector<VertexInputData>& vertexBuffer, vector<int32_t>& indexBuffer, vector<ObjFaceVertexIndices>& objIndices, unordered_map<VertexKey, int32_t>& vertexMap, int32_t i, int32_t i1, int32_t i2, XMFLOAT3& rollingCentroidSum)
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

		rollingCentroidSum = Add(rollingCentroidSum, m_posList.at(indices.PositionIndex));
	}
	else
	{
		int vIndex = vertexMap.at(key);
		indexBuffer.push_back(vIndex);
	}
}

VertexInputData ModelLoader::SetVertexData(ObjFaceVertexIndices i, ObjFaceVertexIndices i1, ObjFaceVertexIndices i2)
{
	VertexInputData vertex;

	vertex.Position = m_posList.at(i.PositionIndex);
	vertex.Texture = m_texList.at(i.TextureIndex);

	XMFLOAT3 normal = m_norList.at(i.NormalIndex);
	vertex.Normal = normal;

	XMFLOAT3 pos1 = m_posList.at(i1.PositionIndex);
	XMFLOAT3 pos2 = m_posList.at(i2.PositionIndex);

	XMFLOAT2 tex1 = m_texList.at(i1.TextureIndex);
	XMFLOAT2 tex2 = m_texList.at(i2.TextureIndex);

	XMFLOAT3 vec1 = Subtract(pos1, vertex.Position);
	XMFLOAT3 vec2 = Subtract(pos2, vertex.Position);

	XMFLOAT2 texVec1 = Subtract(tex1, vertex.Texture);
	XMFLOAT2 texVec2 = Subtract(tex2, vertex.Texture);

	float den = 1.0f / (texVec1.x * texVec2.y + texVec1.y * texVec2.x);

	XMFLOAT3 tangent;

	tangent.x = (texVec2.y * vec1.x - texVec1.y * vec2.x) * den;
	tangent.y = (texVec2.y * vec1.y - texVec1.y * vec2.y) * den;
	tangent.z = (texVec2.y * vec1.z - texVec1.y * vec2.z) * den;

	XMFLOAT3 binormal;

	binormal.x = (texVec1.x * vec2.x - texVec2.x * vec1.x) * den;
	binormal.y = (texVec1.x * vec2.y - texVec2.x * vec1.y) * den;
	binormal.z = (texVec1.x * vec2.z - texVec2.x * vec1.z) * den;

	vertex.Tangent = Normalize(tangent);
	vertex.Binormal = Normalize(binormal);

	return vertex;
}

void ModelLoader::LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<int32_t>& outIndexBuffer, size_t& outVertexCount, size_t& outIndexCount, float& boundingSphereRadius)
{
	ifstream fin;
	wstring longPath = L"Assets/Models/" + filePath;
	fin.open(longPath, std::ios::binary);

	if (!fin)
		throw new std::exception("IO Exception");

	fin.read(reinterpret_cast<char*>(&boundingSphereRadius), sizeof(float));

	fin.read(reinterpret_cast<char*>(&outVertexCount), sizeof(size_t));

	VertexInputData data;
	for (int32_t i = 0; i < outVertexCount; i++)
	{		
		fin.read(reinterpret_cast<char*>(&data), sizeof(VertexInputData));
		outVertexBuffer.push_back(data);
	}

	fin.read(reinterpret_cast<char*>(&outIndexCount), sizeof(size_t));

	int32_t index;
	for (int32_t i = 0; i < outIndexCount; i++)
	{		
		fin.read(reinterpret_cast<char*>(&index), sizeof(int32_t));
		outIndexBuffer.push_back(index);
	}
}

void ModelLoader::LoadSplitModel(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, string name, Batch* batch, shared_ptr<Shader> shader)
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
		}	

		shared_ptr<Model> model = std::make_shared<Model>();
		shared_ptr<Material> material = std::make_shared<Material>(2);
		shared_ptr<Texture> diffuseTex = std::make_shared<Texture>();
		shared_ptr<Texture> normalTex = std::make_shared<Texture>();		

		// TODO: CHANGE THIS TO NOT BE BISTRO
		string modelPath = "Bistro/" + modelName + ".model";
		model->Init(commandList, modelPath);		

		string diffuseTexPath = "Bistro/" + diffuseName;
		bool hasAlpha;
		diffuseTex->Init(d3d, commandList, diffuseTexPath, hasAlpha);

		string normalTexPath = "Bistro/" + normalName;
		normalTex->Init(d3d, commandList, normalTexPath);

		material->AddTexture(d3d, diffuseTex);
		material->AddTexture(d3d, normalTex);

		shared_ptr<GameObject> go = std::make_shared<GameObject>(modelName, model, shader, material);
		batch->AddGameObject(go, hasAlpha);
	}

	fin.close();
}
