#include "pch.h"
#include "ModelLoader.h"
#include "Utils.h"
#include <direct.h>

vector<XMFLOAT3> ModelLoader::m_posList;
vector<XMFLOAT2> ModelLoader::m_texList;
vector<XMFLOAT3> ModelLoader::m_norList;
vector<ObjObject> ModelLoader::m_indexList;

void ModelLoader::PreprocessObjFile(string filePath)
{
	// Goal:
	// Place objects into their own files 
	// Create folder with name of filepath
	// Whenever you encounter o or usemtl then save index list and move onto next one
	// Save name of object with each index list and use that for the file naem
	// Repeat end section for each index list  

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

	while (fin.get(input))
	{
		if (input == 'o')
		{
			if (fin.peek() != ' ')
				continue;

			objectIndex++;
			ObjObject object;

			fin.get();
			fin >> object.Name;
			m_indexList.push_back(object);
			continue;
		}

		if (input == 'f')
		{
			ObjFaceVertexIndices indices;

			for (int i = 0; i < 3; i++)
			{
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
				m_indexList.at(objectIndex).IndexList.push_back(indices);

				auto indicesMinus3 = m_indexList.at(objectIndex).IndexList.at(m_indexList.at(objectIndex).IndexList.size() - 3);
				m_indexList.at(objectIndex).IndexList.push_back(indicesMinus3);

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

		if (input != 'v')
			continue;

		fin.get(input);

		if (input != ' ' && input != 't' && input != 'n')
			continue;

		if (input == 't')
		{
			XMFLOAT2 tex;
			fin >> tex.x;
			fin >> tex.y;
			m_texList.push_back(tex);
			continue;
		}

		XMFLOAT3 vec;
		fin >> vec.x;
		fin >> vec.y;
		fin >> vec.z;

		if (input == 'n')
			m_norList.push_back(vec);
		else
			m_posList.push_back(vec);
	}

	fin.close();

	size_t lastSlashPos = filePath.find_last_of("\\/");
	size_t lastDotPos = filePath.find_last_of(".");
	string folderName = (lastSlashPos != std::string::npos) ? filePath.substr(lastSlashPos + 1, lastDotPos - lastSlashPos - 1) : filePath;
	string folderPath = "Assets/Models/" + folderName;
	int result = _mkdir(folderPath.c_str());
	if (result != 0)
		throw new std::exception("IO Exception (OUTPUT)");

	for (size_t i = 0; i < m_indexList.size(); i++)
	{
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
	unordered_map<VertexKey, int> vertexMap;
	vector<int> indexBuffer;

	for (int i = 0; i < vertexCount - 2; i += 3)
	{
		int i1 = i + 1;
		int i2 = i + 2;

		TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i, i1, i2);
		TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i1, i, i2);
		TryAddVertex(vertexBuffer, indexBuffer, objIndices, vertexMap, i2, i1, i);
	}

	ofstream fout;
	std::ios_base::openmode openFlags = std::ios::binary | std::ios::out | std::ios::trunc;
	fout.open(outputPath, openFlags);
	if (!fout)
		throw new std::exception("IO Exception (OUTPUT)");

	size_t vertexCountUINT = vertexBuffer.size();
	fout.write(reinterpret_cast<const char*>(&vertexCountUINT), sizeof(size_t));

	for (int i = 0; i < vertexBuffer.size(); i++)
		fout.write(reinterpret_cast<const char*>(&vertexBuffer.at(i)), sizeof(VertexInputData));

	size_t indexCountUINT = indexBuffer.size();
	fout.write(reinterpret_cast<const char*>(&indexCountUINT), sizeof(size_t));

	for (int i = 0; i < indexBuffer.size(); i++)
		fout.write(reinterpret_cast<const char*>(&indexBuffer.at(i)), sizeof(int));

	fout.close();

	if (!fout.good())
		throw new std::exception("IO Exception (OUTPUT)");
}

void ModelLoader::TryAddVertex(vector<VertexInputData>& vertexBuffer, vector<int>& indexBuffer, vector<ObjFaceVertexIndices>& objIndices, unordered_map<VertexKey, int>& vertexMap, int i, int i1, int i2)
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

void ModelLoader::LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<WORD>& outIndexBuffer, size_t& outVertexCount, size_t& outIndexCount)
{
	ifstream fin;
	wstring longPath = L"Assets/Models/" + filePath;
	fin.open(longPath, std::ios::binary);

	if (!fin)
		throw new std::exception("IO Exception");

	fin.read(reinterpret_cast<char*>(&outVertexCount), sizeof(size_t));

	VertexInputData data;
	for (int i = 0; i < outVertexCount; i++)
	{		
		fin.read(reinterpret_cast<char*>(&data), sizeof(VertexInputData));
		outVertexBuffer.push_back(data);
	}

	fin.read(reinterpret_cast<char*>(&outIndexCount), sizeof(size_t));

	int index;
	for (int i = 0; i < outIndexCount; i++)
	{		
		fin.read(reinterpret_cast<char*>(&index), sizeof(int));
		outIndexBuffer.push_back((WORD)index);
	}
}
