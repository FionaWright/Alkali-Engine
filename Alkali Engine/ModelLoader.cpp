#include "pch.h"
#include "ModelLoader.h"
#include "Utils.h"
#include <direct.h>
#include <filesystem>

#include "fastgltf/include/fastgltf/tools.hpp"

namespace filesystem = std::filesystem;

vector<XMFLOAT3> ModelLoader::ms_posList;
vector<XMFLOAT2> ModelLoader::ms_texList;
vector<XMFLOAT3> ModelLoader::ms_norList;
vector<ObjObject> ModelLoader::ms_indexList;
fastgltf::Parser ModelLoader::ms_parser;

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
					objectIndex = i;
					foundExistingObject = true;
					break;
				}
			}
			if (foundExistingObject)
				continue;

			ms_indexList.push_back(object);
			objectIndex = ms_indexList.size() - 1;

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
		string outputPath = folderPath + ".model";

		SaveObject(outputPath, ms_indexList.at(0).IndexList);

		ms_posList.clear();
		ms_texList.clear();
		ms_norList.clear();
		return;
	}

	for (size_t i = 0; i < ms_indexList.size(); i++)
	{
		if (ms_indexList.at(i).IndexList.size() == 0)
			continue;

		string outputPath = folderPath + "/" + ms_indexList.at(i).Name + ".model";

		SaveObject(outputPath, ms_indexList.at(i).IndexList);
	}

	ms_posList.clear();
	ms_texList.clear();
	ms_norList.clear();
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

		XMFLOAT2 t0 = ms_texList.at(objIndices.at(i).TextureIndex);
		XMFLOAT2 t1 = ms_texList.at(objIndices.at(i1).TextureIndex);
		XMFLOAT2 t2 = ms_texList.at(objIndices.at(i2).TextureIndex);

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

		rollingCentroidSum = Add(rollingCentroidSum, ms_posList.at(indices.PositionIndex));
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

void ModelLoader::LoadModel(wstring filePath, vector<VertexInputData>& outVertexBuffer, vector<int32_t>& outIndexBuffer, float& boundingSphereRadius, XMFLOAT3& centroid)
{
	ifstream fin;
	wstring longPath = L"Assets/Models/" + filePath;
	fin.open(longPath, std::ios::binary);

	size_t vertexCount, indexCount;

	if (!fin)
		throw new std::exception("IO Exception");

	fin.read(reinterpret_cast<char*>(&boundingSphereRadius), sizeof(float));
	fin.read(reinterpret_cast<char*>(&centroid), sizeof(XMFLOAT3));

	fin.read(reinterpret_cast<char*>(&vertexCount), sizeof(size_t));

	VertexInputData data;
	for (int32_t i = 0; i < vertexCount; i++)
	{		
		fin.read(reinterpret_cast<char*>(&data), sizeof(VertexInputData));
		outVertexBuffer.push_back(data);
	}

	fin.read(reinterpret_cast<char*>(&indexCount), sizeof(size_t));

	int32_t index;
	for (int32_t i = 0; i < indexCount; i++)
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
		shared_ptr<Material> material = std::make_shared<Material>(2);
		shared_ptr<Texture> diffuseTex = std::make_shared<Texture>();
		shared_ptr<Texture> normalTex = std::make_shared<Texture>();		

		string modelPath = name + "/" + modelName + ".model";
		model->Init(commandList, modelPath);		
		
		string diffuseTexPath = name + "/" + diffuseName;
		if (diffuseName == "")
			diffuseTexPath = "WhitePOT.tga";

		bool hasAlpha;
		diffuseTex->Init(d3d, commandList, diffuseTexPath, hasAlpha);

		string normalTexPath = name + "/" + normalName;
		if (normalName == "")
			normalTexPath = "DefaultNormal.tga";

		normalTex->Init(d3d, commandList, normalTexPath);

		material->AddTexture(d3d, diffuseTex);
		material->AddTexture(d3d, normalTex);

		shared_ptr<GameObject> go = std::make_shared<GameObject>(modelName, model, shader, material);
		batch->AddGameObject(go, hasAlpha);
	}

	fin.close();
}

template<typename Func>
void LoadGLTFVertexData(vector<VertexInputData>& vBuffer, fastgltf::Expected<fastgltf::Asset>& asset, const fastgltf::Primitive& primitive, const char* attribute, Func func)
{
	const auto& accessor = asset->accessors[primitive.findAttribute(attribute)->second];
	const auto& bufferView = asset->bufferViews[*accessor.bufferViewIndex];
	const auto& bufferData = asset->buffers[bufferView.bufferIndex].data;

	const size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
	const size_t byteSize = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
	const size_t dataStride = bufferView.byteStride.value_or(byteSize);

	const uint8_t* pData = nullptr;
	vector<uint8_t> pTempFileData;

	if (bufferData.index() == 3)
		pData = std::get<fastgltf::sources::Array>(bufferData).bytes.data() + dataOffset;
	else if (bufferData.index() == 2)
	{
		auto& uri = std::get<fastgltf::sources::URI>(bufferData);
		string path(uri.uri.path());
		std::ifstream file("Assets/Models/" + path, std::ios::binary);

		if (!file.is_open())
			throw std::runtime_error("Failed to open file");

		file.seekg(dataOffset + uri.fileByteOffset, std::ios::beg);

		size_t totalBytes = accessor.count * byteSize;
		pTempFileData.resize(totalBytes);
		file.read(reinterpret_cast<char*>(pTempFileData.data()), totalBytes);
		file.close();

		pData = pTempFileData.data();

		if (!file)
			throw std::runtime_error("Failed to read the required data from file.");
	}
	else
		throw new std::exception("Invalid buffer data type");

	if (vBuffer.size() < accessor.count)
		vBuffer.resize(accessor.count);

	for (size_t i = 0; i < vBuffer.size(); ++i)
	{	
		auto address = pData + i * dataStride;
		func(address, &vBuffer[i]);
	}
}

void LoadGLTFIndices(vector<uint32_t>& iBuffer, fastgltf::Expected<fastgltf::Asset>& asset, const fastgltf::Primitive& primitive)
{
	const auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
	const auto& bufferView = asset->bufferViews[*accessor.bufferViewIndex];
	const auto& bufferData = asset->buffers[bufferView.bufferIndex].data;

    const size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
	const size_t indexByteSize = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
	const size_t dataStride = bufferView.byteStride.value_or(indexByteSize);

	const uint8_t* pData = nullptr;
	vector<uint8_t> pTempFileData;

	if (bufferData.index() == 3)
		pData = std::get<fastgltf::sources::Array>(bufferData).bytes.data() + dataOffset;
	else if (bufferData.index() == 2)
	{
		auto& uri = std::get<fastgltf::sources::URI>(bufferData);
		string path(uri.uri.path());
		std::ifstream file("Assets/Models/" + path, std::ios::binary);
		
		if (!file.is_open())
			throw std::runtime_error("Failed to open file");

		file.seekg(dataOffset + uri.fileByteOffset, std::ios::beg);

		size_t totalBytes = accessor.count * indexByteSize;
		pTempFileData.resize(totalBytes);
		file.read(reinterpret_cast<char*>(pTempFileData.data()), totalBytes);
		file.close();

		pData = pTempFileData.data();

		if (!file)
			throw std::runtime_error("Failed to read the required data from file.");
	}
	else
		throw new std::exception("Invalid buffer data type");

	iBuffer.resize(accessor.count);

	if (indexByteSize == 4)
	{
		std::memcpy(iBuffer.data(), pData, accessor.count * 4);
		return;
	}

	for (size_t i = 0; i < iBuffer.size(); ++i)
	{
		const uint8_t* const indexData = pData + i * dataStride;

		switch (indexByteSize)
		{
		case 1:
			iBuffer[i] = *indexData;
			break;

		case 2:
			uint16_t value16;
			std::memcpy(&value16, indexData, sizeof(uint16_t));
			iBuffer[i] = value16;
			break;

		default:
			throw std::invalid_argument("Error: Unexpected indices data size (" + std::to_string(indexByteSize) + ").");
		}
	}
}

void ModelLoader::LoadModelGLTF(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, string name, Batch* batch, shared_ptr<Shader> shader, vector<string> nameWhiteList)
{
	string path = "Assets/Models/" + name;

	// The GltfDataBuffer class contains static factories which create a buffer for holding the
	// glTF data. These return Expected<GltfDataBuffer>, which can be checked if an error occurs.
	// The parser accepts any subtype of GltfDataGetter, which defines an interface for reading
	// chunks of the glTF file for the Parser to handle. fastgltf provides a few predefined classes
	// which inherit from GltfDataGetter, so choose whichever fits your usecase the best.
	fastgltf::Expected<fastgltf::GltfDataBuffer> data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None)
		throw new std::exception("FastGLTF error");

	fastgltf::Options options = fastgltf::Options::None;

	fastgltf::Expected<fastgltf::Asset> asset = ms_parser.loadGltf(data.get(), path, options);
	auto error = asset.error();
	if (error != fastgltf::Error::None)
		throw new std::exception("FastGLTF error");
	
	error = fastgltf::validate(asset.get());
	if (error != fastgltf::Error::None)
		throw new std::exception("FastGLTF error");

	for (int i = 0; i < asset->nodes.size(); i++)
	{		
		auto& node = asset->nodes.at(i);		
		if (!node.meshIndex.has_value())
			continue;

		size_t meshIndex = node.meshIndex.value();
		auto& mesh = asset->meshes.at(meshIndex);

		string name(node.name);
		if (nameWhiteList.size() > 0)
		{
			bool found = false;
			for (int j = 0; j < nameWhiteList.size(); j++)
			{
				if (name.starts_with(nameWhiteList[j]))
				{
					found = true;
					break;
				}
			}

			if (!found)
				continue;
		}

		for (const auto& primitive : mesh.primitives)
		{
			vector<VertexInputData> vertexBuffer;

			LoadGLTFVertexData(vertexBuffer, asset, primitive, "POSITION", [](const uint8_t* address, VertexInputData* output) {
				// TODO: Calculate bounding sphere stuff 
				output->Position = *reinterpret_cast<const XMFLOAT3*>(address);
				//if (RIGHT_HANDED_TO_LEFT)
				//	output->Position.x = -output->Position.x;
			});

			LoadGLTFVertexData(vertexBuffer, asset, primitive, "TEXCOORD_0", [](const uint8_t* address, VertexInputData* output) {
				output->Texture = *reinterpret_cast<const XMFLOAT2*>(address);
			});

			LoadGLTFVertexData(vertexBuffer, asset, primitive, "NORMAL", [](const uint8_t* address, VertexInputData* output) {
				output->Normal = *reinterpret_cast<const XMFLOAT3*>(address);
				//if (RIGHT_HANDED_TO_LEFT)
				//	output->Normal.x = -output->Normal.x;
			});

			LoadGLTFVertexData(vertexBuffer, asset, primitive, "TANGENT", [](const uint8_t* address, VertexInputData* output) {
				const XMFLOAT4* data = reinterpret_cast<const XMFLOAT4*>(address);
				float handedness = data->w > 0 ? 1 : -1;
				output->Tangent = Mult(XMFLOAT3(data->x, data->y, data->z), handedness);
			});

			for (size_t j = 0; j < vertexBuffer.size(); ++j)
			{
				vertexBuffer[j].Binormal = Cross(vertexBuffer[j].Tangent, vertexBuffer[j].Normal);
			}

			vector<uint32_t> indexBuffer;

			LoadGLTFIndices(indexBuffer, asset, primitive);

			fastgltf::Material& mat = asset->materials[primitive.materialIndex.value_or(0)];		

			shared_ptr<Model> model = std::make_shared<Model>();
			shared_ptr<Material> material = std::make_shared<Material>(2);
			shared_ptr<Texture> diffuseTex = std::make_shared<Texture>();
			shared_ptr<Texture> normalTex = std::make_shared<Texture>();

			model->Init(vertexBuffer.size(), indexBuffer.size(), sizeof(VertexInputData), 999999999999, XMFLOAT3_ZERO);
			model->SetBuffers(commandList, vertexBuffer.data(), indexBuffer.data());

			string diffuseTexPath = "";
			if (diffuseTexPath == "")
				diffuseTexPath = "WhitePOT.tga";

			bool hasAlpha;
			diffuseTex->Init(d3d, commandList, diffuseTexPath, hasAlpha);

			string normalTexPath = "";
			if (normalTexPath == "")
				normalTexPath = "DefaultNormal.tga";

			normalTex->Init(d3d, commandList, normalTexPath);

			material->AddTexture(d3d, diffuseTex);
			material->AddTexture(d3d, normalTex);

			shared_ptr<GameObject> go = std::make_shared<GameObject>(name, model, shader, material);
			auto& pos = std::get<fastgltf::TRS>(node.transform).translation;
			go->SetPosition(pos.x(), pos.y(), pos.z());

			auto& rot = std::get<fastgltf::TRS>(node.transform).rotation;
			//go->SetRotation(rot.x(), rot.y(), rot.z());

			auto& scale = std::get<fastgltf::TRS>(node.transform).rotation;
			//go->SetScale(scale.x(), scale.y(), scale.z());
			go->SetScale(0.05f);

			batch->AddGameObject(go, hasAlpha);
		}
	}
}