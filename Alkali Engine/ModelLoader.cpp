#include "pch.h"
#include "ModelLoader.h"

vector<XMFLOAT3> ModelLoader::m_posList;
vector<XMFLOAT2> ModelLoader::m_texList;
vector<XMFLOAT3> ModelLoader::m_norList;
vector<ObjIndexGroup> ModelLoader::m_indexList;

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

XMFLOAT3 Subtract(XMFLOAT3 a, XMFLOAT3 b)
{
	return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

XMFLOAT2 Subtract(XMFLOAT2 a, XMFLOAT2 b)
{
	return XMFLOAT2(a.x - b.x, a.y - b.y);
}

XMFLOAT3 Normalize(const XMFLOAT3& v)
{
	float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return XMFLOAT3(v.x / length, v.y / length, v.z / length);
}

bool Equals(XMFLOAT3 a, XMFLOAT3 b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

void ModelLoader::PreprocessObjFile(wstring filePath)
{
	ifstream fin;
	char input = '0';

	fin.open(filePath);

	if (fin.fail())
		throw new std::exception("IO Exception");

	m_posList.clear();
	m_texList.clear();
	m_norList.clear();
	m_indexList.clear();

	while (fin.get(input))
	{
		if (input == 'f')
		{
			for (int i = 0; fin.peek() == ' '; i++)
			{
				ObjIndexGroup indices;

				fin >> indices.PositionIndex;
				indices.PositionIndex--;

				if (fin.peek() == '/')
					fin.get();

				if (fin.peek() == '/')
					throw new std::exception("Invalid model data, make sure to triangulate");

				fin >> indices.TextureIndex;
				indices.TextureIndex--;

				if (fin.peek() == '/')
					fin.get();

				fin >> indices.NormalIndex;
				indices.NormalIndex--;

				m_indexList.push_back(indices);
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

	int vertexCount = m_indexList.size();

	vector<VertexInputData> vertexBuffer;
	unordered_map<VertexKey, int> vertexMap;
	vector<int> indexBuffer;
	
	for (int i = 0; i < vertexCount - 2; i += 3)
	{
		int i1 = i + 1;
		int i2 = i + 2;

		VertexInputData data;
		VertexKey key;
		ObjIndexGroup indices;

		indices = m_indexList.at(i);
		key = { indices.PositionIndex, indices.NormalIndex };

		if (!vertexMap.contains(key))
		{		
			data = SetVertexData(indices, i1, i2);
			vertexBuffer.push_back(data);

			int vBufferIndex = vertexBuffer.size() - 1;
			indexBuffer.push_back(vBufferIndex);			
			vertexMap.emplace(key, vBufferIndex);
		}
		else
		{
			int vIndex = vertexMap.at(key);
			indexBuffer.push_back(vIndex);
		}			

		indices = m_indexList.at(i1);
		key = { indices.PositionIndex, indices.NormalIndex };

		if (!vertexMap.contains(key))
		{
			data = SetVertexData(indices, i, i2);
			vertexBuffer.push_back(data);

			int vBufferIndex = vertexBuffer.size() - 1;
			indexBuffer.push_back(vBufferIndex);
			vertexMap.emplace(key, vBufferIndex);
		}
		else
		{
			int vIndex = vertexMap.at(key);
			indexBuffer.push_back(vIndex);
		}

		indices = m_indexList.at(i2);
		key = { indices.PositionIndex, indices.NormalIndex };

		if (!vertexMap.contains(key))
		{
			data = SetVertexData(indices, i1, i);
			vertexBuffer.push_back(data);

			int vBufferIndex = vertexBuffer.size() - 1;
			indexBuffer.push_back(vBufferIndex);
			vertexMap.emplace(key, vBufferIndex);
		}
		else
		{
			int vIndex = vertexMap.at(key);
			indexBuffer.push_back(vIndex);
		}
	}

	m_posList.clear();
	m_texList.clear();
	m_norList.clear();
	m_indexList.clear();

	size_t lastSlashPos = filePath.find_last_of(L"\\/");
	size_t lastDotPos = filePath.find_last_of(L".");
	wstring name = (lastSlashPos != std::string::npos) ? filePath.substr(lastSlashPos + 1, lastDotPos - lastSlashPos - 1) : filePath;

	wstring outputPath = L"Assets/Models/" + name + L".model";

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

VertexInputData ModelLoader::SetVertexData(ObjIndexGroup indices, int otherVertexInFaceIndex1, int otherVertexInFaceIndex2)
{
	VertexInputData vertex;

	ObjIndexGroup indices1 = m_indexList.at(otherVertexInFaceIndex1);
	ObjIndexGroup indices2 = m_indexList.at(otherVertexInFaceIndex2);

	vertex.Position = m_posList.at(indices.PositionIndex);
	vertex.Texture = m_texList.at(indices.TextureIndex);

	XMFLOAT3 normal = m_norList.at(indices.NormalIndex);
	vertex.Normal = normal;

	XMFLOAT3 pos1 = m_posList.at(indices1.PositionIndex);
	XMFLOAT3 pos2 = m_posList.at(indices2.PositionIndex);

	XMFLOAT2 tex1 = m_texList.at(indices1.TextureIndex);
	XMFLOAT2 tex2 = m_texList.at(indices2.TextureIndex);

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
