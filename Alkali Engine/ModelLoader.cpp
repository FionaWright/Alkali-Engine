#include "pch.h"
#include "ModelLoader.h"
#include "Utils.h"
#include <direct.h>
#include <filesystem>

#include "ResourceTracker.h"

#include "fastgltf/include/fastgltf/tools.hpp"

namespace filesystem = std::filesystem;

vector<XMFLOAT3> ModelLoader::ms_posList;
vector<XMFLOAT2> ModelLoader::ms_texList;
vector<XMFLOAT3> ModelLoader::ms_norList;
vector<ObjObject> ModelLoader::ms_indexList;
fastgltf::Parser ModelLoader::ms_parser;

constexpr bool RIGHT_HANDED_TO_LEFT = true;

Transform ToTransform(fastgltf::TRS& trs)
{
	Transform transform;

	auto& pos = trs.translation;
	transform.Position = { pos.x(), pos.y(), pos.z() };

	auto& rot = trs.rotation;
	XMFLOAT4 rotFloat4 = XMFLOAT4(rot.x(), rot.y(), rot.z(), rot.w());
	XMVECTOR rotVec = XMLoadFloat4(&rotFloat4);
	XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(rotVec);
	float pitch = std::atan2(rotationMatrix.r[1].m128_f32[2], rotationMatrix.r[2].m128_f32[2]);
	float yaw = std::atan2(-rotationMatrix.r[0].m128_f32[2], std::sqrt(rotationMatrix.r[1].m128_f32[2] * rotationMatrix.r[1].m128_f32[2] + rotationMatrix.r[2].m128_f32[2] * rotationMatrix.r[2].m128_f32[2]));
	float roll = std::atan2(rotationMatrix.r[0].m128_f32[1], rotationMatrix.r[0].m128_f32[0]);
	transform.Rotation = { pitch, yaw, roll };

	auto& scale = trs.scale;
	transform.Scale = { scale.x(), scale.y(), scale.z() };

	return transform;
}

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

	outVertexBuffer.resize(vertexCount);
	fin.read(reinterpret_cast<char*>(outVertexBuffer.data()), sizeof(VertexInputData) * vertexCount);

	fin.read(reinterpret_cast<char*>(&indexCount), sizeof(size_t));

	outIndexBuffer.resize(indexCount);
	fin.read(reinterpret_cast<char*>(outIndexBuffer.data()), sizeof(int32_t) * indexCount);
}

void ModelLoader::LoadSplitModel(D3DClass* d3d, RootParamInfo& rpi, ID3D12GraphicsCommandList2* commandList, string name, Batch* batch, shared_ptr<Shader> shader)
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
		model->Init(commandList, modelPath);

		string diffuseTexPath = name + "/" + diffuseName;
		if (diffuseName == "")
			diffuseTexPath = "WhitePOT.tga";

		shared_ptr<Texture> diffuseTex;
		if (!ResourceTracker::TryGetTexture(diffuseTexPath, diffuseTex))
		{
			diffuseTex->Init(d3d, commandList, diffuseTexPath);
		}

		string normalTexPath = name + "/" + normalName;
		if (normalName == "")
			normalTexPath = "DefaultNormal.tga";

		shared_ptr<Texture> normalTex;
		if (!ResourceTracker::TryGetTexture(normalTexPath, normalTex))
		{
			normalTex->Init(d3d, commandList, normalTexPath);
		}

		vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
		vector<UINT> cbvSizesFrame = { sizeof(CameraCB), sizeof(DirectionalLightCB) };
		vector<Texture*> textures = { diffuseTex.get(), normalTex.get() };

		shared_ptr<Material> material = std::make_shared<Material>();
		material->AddCBVs(d3d, commandList, cbvSizesDraw, false);
		material->AddCBVs(d3d, commandList, cbvSizesFrame, true);
		material->AddSRVs(d3d, textures);
		ResourceTracker::AddMaterial(material);

		GameObject go(modelName, rpi, model, shader, material);
		batch->AddGameObject(go);
	}

	fin.close();
}

template<typename Func>
void LoadGLTFVertexData(vector<VertexInputData>& vBuffer, fastgltf::Expected<fastgltf::Asset>& asset, const fastgltf::Primitive& primitive, const char* attribute, Func func)
{
	const auto attributeObj = primitive.findAttribute(attribute);
	if (attributeObj == primitive.attributes.cend())
		throw std::exception("Err");

	const auto& accessor = asset->accessors.at(attributeObj->second);
	const auto& bufferView = asset->bufferViews.at(*accessor.bufferViewIndex);
	const auto& bufferData = asset->buffers.at(bufferView.bufferIndex).data;

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

	if (!RIGHT_HANDED_TO_LEFT && indexByteSize == 4)
	{
		std::memcpy(iBuffer.data(), pData, accessor.count * 4);
		return;
	}

	for (size_t i = 0; i < iBuffer.size(); i += 3)
	{
		const uint8_t* const indexData0 = pData + (i + 0) * dataStride;
		const uint8_t* const indexData1 = pData + (i + 1) * dataStride;
		const uint8_t* const indexData2 = pData + (i + 2) * dataStride;

		switch (indexByteSize)
		{
		case 1:
			if (RIGHT_HANDED_TO_LEFT)
			{
				iBuffer[i + 2] = *indexData0;
				iBuffer[i + 1] = *indexData1;
				iBuffer[i + 0] = *indexData2;
				break;
			}

			iBuffer[i + 0] = *indexData0;
			iBuffer[i + 1] = *indexData1;
			iBuffer[i + 2] = *indexData2;
			break;

		case 2:
			uint16_t value16;

			if (RIGHT_HANDED_TO_LEFT)
			{
				std::memcpy(&value16, indexData0, sizeof(uint16_t));
				iBuffer[i + 2] = value16;
				std::memcpy(&value16, indexData1, sizeof(uint16_t));
				iBuffer[i + 1] = value16;
				std::memcpy(&value16, indexData2, sizeof(uint16_t));
				iBuffer[i + 0] = value16;
				break;
			}

			std::memcpy(&value16, indexData0, sizeof(uint16_t));
			iBuffer[i + 0] = value16;
			std::memcpy(&value16, indexData1, sizeof(uint16_t));
			iBuffer[i + 1] = value16;
			std::memcpy(&value16, indexData2, sizeof(uint16_t));
			iBuffer[i + 2] = value16;
			break;
		
		case 4: // Only reached when converting RHS -> LHS
			std::memcpy(&iBuffer[i + 2], indexData0, sizeof(uint32_t));
			std::memcpy(&iBuffer[i + 1], indexData1, sizeof(uint32_t));
			std::memcpy(&iBuffer[i + 0], indexData2, sizeof(uint32_t));
			break;

		default:
			throw std::invalid_argument("Error: Unexpected indices data size (" + std::to_string(indexByteSize) + ").");
		}
	}
}

void LoadModel(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, fastgltf::Expected<fastgltf::Asset>& asset, const fastgltf::Primitive& primitive, shared_ptr<Model> model)
{
	vector<VertexInputData> vertexBuffer;

	LoadGLTFVertexData(vertexBuffer, asset, primitive, "POSITION", [](const uint8_t* address, VertexInputData* output) {
		output->Position = *reinterpret_cast<const XMFLOAT3*>(address);
		if (RIGHT_HANDED_TO_LEFT)
			output->Position.x = -output->Position.x;
		});

	size_t vertexCount = vertexBuffer.size();
	if (vertexCount == 0)
		return;

	LoadGLTFVertexData(vertexBuffer, asset, primitive, "TEXCOORD_0", [](const uint8_t* address, VertexInputData* output) {
		output->Texture = *reinterpret_cast<const XMFLOAT2*>(address);
		});

	LoadGLTFVertexData(vertexBuffer, asset, primitive, "NORMAL", [](const uint8_t* address, VertexInputData* output) {
		output->Normal = Normalize(*reinterpret_cast<const XMFLOAT3*>(address));
		if (RIGHT_HANDED_TO_LEFT)
			output->Normal.x = -output->Normal.x;
		});

	LoadGLTFVertexData(vertexBuffer, asset, primitive, "TANGENT", [](const uint8_t* address, VertexInputData* output) {
		const XMFLOAT4* data = reinterpret_cast<const XMFLOAT4*>(address);
		float handedness = data->w > 0 ? 1 : -1;
		output->Tangent = Normalize(Mult(XMFLOAT3(data->x, data->y, data->z), handedness));
		if (RIGHT_HANDED_TO_LEFT)
			output->Tangent.x = -output->Tangent.x;
		});

	float boundingRadiusSq = 0;
	struct Double3
	{
		double X = 0, Y = 0, Z = 0;
	} rollingCentroidSum;

	for (size_t j = 0; j < vertexCount; j++)
	{
		vertexBuffer[j].Binormal = Normalize(Cross(vertexBuffer[j].Tangent, vertexBuffer[j].Normal));	
		//if (Dot(Cross(vertexBuffer[j].Tangent, vertexBuffer[j].Binormal), vertexBuffer[j].Normal) < 0.0f)
		//	vertexBuffer[j].Binormal = Negate(vertexBuffer[j].Binormal);

		rollingCentroidSum.X += vertexBuffer[j].Position.x;
		rollingCentroidSum.Y += vertexBuffer[j].Position.y;
		rollingCentroidSum.Z += vertexBuffer[j].Position.z;
	}

	rollingCentroidSum.X /= vertexCount;
	rollingCentroidSum.Y /= vertexCount;
	rollingCentroidSum.Z /= vertexCount;
	XMFLOAT3 centroidFloat3 = XMFLOAT3(rollingCentroidSum.X, rollingCentroidSum.Y, rollingCentroidSum.Z);

	for (size_t j = 0; j < vertexCount; j++)
	{
		XMFLOAT3 diff = Subtract(centroidFloat3, vertexBuffer[j].Position);
		float magSq = Dot(diff, diff);
		if (magSq > boundingRadiusSq)
			boundingRadiusSq = magSq;
	}

	boundingRadiusSq = std::sqrt(boundingRadiusSq);

	vector<uint32_t> indexBuffer;
	LoadGLTFIndices(indexBuffer, asset, primitive);

	model->Init(vertexBuffer.size(), indexBuffer.size(), sizeof(VertexInputData), boundingRadiusSq, centroidFloat3);
	model->SetBuffers(commandList, vertexBuffer.data(), indexBuffer.data());
}

void LoadPrimitive(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, RootParamInfo& rpi, fastgltf::Expected<fastgltf::Asset>& asset, const fastgltf::Primitive& primitive, Batch* batch, shared_ptr<Shader> shader, shared_ptr<Shader> shaderCullOff, string modelNameExtensionless, fastgltf::Node& node, Transform& transform, string id)
{
	shared_ptr<Model> model;
	if (!ResourceTracker::TryGetModel(id, model))
	{
		LoadModel(d3d, commandList, asset, primitive, model);
	}

	fastgltf::Material& mat = asset->materials[primitive.materialIndex.value_or(0)];

	string diffuseTexPath = "";
	if (mat.pbrData.baseColorTexture.has_value())
	{
		fastgltf::Texture& tex = asset->textures[mat.pbrData.baseColorTexture.value().textureIndex];
		fastgltf::Image& image = asset->images[tex.imageIndex.value()];

		if (image.data.index() == 2)
		{
			string texName(std::get<fastgltf::sources::URI>(image.data).uri.path());
			size_t slashIndex = texName.find_last_of('/');

			if (slashIndex != std::string::npos)
				diffuseTexPath = texName.substr(slashIndex + 1, texName.size() - slashIndex - 1);
			else
				diffuseTexPath = texName;
		}
		else
			throw std::exception("No support for this type of texture");
	}

	string normalTexPath = "";
	if (mat.normalTexture.has_value())
	{
		fastgltf::Texture& tex = asset->textures[mat.normalTexture.value().textureIndex];
		fastgltf::Image& image = asset->images[tex.imageIndex.value()];

		if (image.data.index() == 2)
		{
			string texName(std::get<fastgltf::sources::URI>(image.data).uri.path());
			size_t slashIndex = texName.find_last_of('/');

			if (slashIndex != std::string::npos)
				normalTexPath = texName.substr(slashIndex + 1, texName.size() - slashIndex - 1);
			else
				normalTexPath = texName;
		}
		else
			throw std::exception("No support for this type of texture");
	}

	string texFullFilePath = modelNameExtensionless + "/" + diffuseTexPath;
	if (diffuseTexPath == "")
		texFullFilePath = "WhitePOT.png";

	shared_ptr<Texture> diffuseTex;
	if (!ResourceTracker::TryGetTexture(texFullFilePath, diffuseTex))
	{
		diffuseTex->Init(d3d, commandList, texFullFilePath);
	}
	
	string texNormalFullFilePath = modelNameExtensionless + "/" + normalTexPath;
	if (normalTexPath == "")
		texNormalFullFilePath = "DefaultNormal.tga";

	shared_ptr<Texture> normalTex;
	if (!ResourceTracker::TryGetTexture(texNormalFullFilePath, normalTex))
	{
		normalTex->Init(d3d, commandList, texNormalFullFilePath, false, true);
	}

	vector<UINT> cbvSizesDraw = { sizeof(MatricesCB) };
	vector<UINT> cbvSizesFrame = {sizeof(CameraCB), sizeof(DirectionalLightCB) };
	vector<Texture*> textures = { diffuseTex.get(), normalTex.get() };

	shared_ptr<Material> material = std::make_shared<Material>();
	material->AddCBVs(d3d, commandList, cbvSizesDraw, false);
	material->AddCBVs(d3d, commandList, cbvSizesFrame, true);
	material->AddSRVs(d3d, textures);
	ResourceTracker::AddMaterial(material);

	string nodeName(node.name);
	nodeName = modelNameExtensionless + "::" + nodeName;

	auto shaderUsed = material->GetHasAlpha() ? shaderCullOff : shader;
	GameObject go(nodeName, rpi, model, shaderUsed, material);
	go.SetTransform(transform);

	batch->AddGameObject(go);
}

void LoadNode(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, RootParamInfo& rpi, fastgltf::Expected<fastgltf::Asset>& asset, Batch* batch, shared_ptr<Shader> shader, shared_ptr<Shader> shaderCullOff, vector<string>* nameWhiteList, string modelNameExtensionless, fastgltf::Node& node, Transform& rollingTransform)
{
	fastgltf::TRS& trs = std::get<fastgltf::TRS>(node.transform);
	Transform transform = ToTransform(trs);
	transform.Position = Add(transform.Position, rollingTransform.Position);
	transform.Rotation = Add(transform.Rotation, rollingTransform.Rotation);
	transform.Scale = Mult(transform.Scale, rollingTransform.Scale);

	size_t childCount = node.children.size();
	for (size_t i = 0; i < childCount; i++)
	{
		fastgltf::Node& childNode = asset->nodes[node.children[i]];
		LoadNode(d3d, commandList, rpi, asset, batch, shader, shaderCullOff, nameWhiteList, modelNameExtensionless, childNode, transform);
	}

	if (!node.meshIndex.has_value())
		return;

	string nodeName(node.name);
	if (nameWhiteList && nameWhiteList->size() > 0)
	{
		bool found = false;
		for (int j = 0; j < nameWhiteList->size(); j++)
		{
			if (nodeName.starts_with(nameWhiteList->at(j)))
			{
				found = true;
				break;
			}
		}

		if (!found)
			return;
	}

	size_t meshIndex = node.meshIndex.value();
	fastgltf::Mesh& mesh = asset->meshes.at(meshIndex);

	for (size_t i = 0; i < mesh.primitives.size(); i++)
	{
		std::string id = modelNameExtensionless + "::NODE(" + std::to_string(meshIndex) + ")::PRIMITIVE(" + std::to_string(i) + ")";
		LoadPrimitive(d3d, commandList, rpi, asset, mesh.primitives[i], batch, shader, shaderCullOff, modelNameExtensionless, node, transform, id);
	}
}

void ModelLoader::LoadSplitModelGLTF(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, string modelName, RootParamInfo& rpi, Batch* batch, shared_ptr<Shader> shader, shared_ptr<Shader> shaderCullOff, vector<string>* nameWhiteList)
{
	string path = "Assets/Models/" + modelName;

	size_t dotIndex = modelName.find_last_of('.');
	if (dotIndex == string::npos)
		throw std::exception("Invalid model name");

	string modelNameExtensionless = modelName.substr(0, dotIndex);

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

	for (int i = 0; i < asset->scenes.size(); i++)
	{
		fastgltf::Scene& scene = asset->scenes[i];

		size_t nodeCount = scene.nodeIndices.size();
		for (size_t n = 0; n < nodeCount; n++)
		{
			int nodeIndex = scene.nodeIndices[n];
			fastgltf::Node& node = asset->nodes[nodeIndex];
			Transform defTransform = {};
			LoadNode(d3d, commandList, rpi, asset, batch, shader, shaderCullOff, nameWhiteList, modelNameExtensionless, node, defTransform);
		}
	}
}

void LoadModelsFromNode(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, fastgltf::Expected<fastgltf::Asset>& asset, string modelNameExtensionless, fastgltf::Node& node, vector<shared_ptr<Model>>& modelList)
{
	size_t childCount = node.children.size();
	for (size_t i = 0; i < childCount; i++)
	{
		fastgltf::Node& childNode = asset->nodes[node.children[i]];
		LoadModelsFromNode(d3d, commandList, asset, modelNameExtensionless, childNode, modelList);
	}

	if (!node.meshIndex.has_value())
		return;

	size_t meshIndex = node.meshIndex.value();
	fastgltf::Mesh& mesh = asset->meshes.at(meshIndex);

	for (size_t i = 0; i < mesh.primitives.size(); i++)
	{
		std::string id = modelNameExtensionless + "::NODE(" + std::to_string(meshIndex) + ")::PRIMITIVE(" + std::to_string(i) + ")";
		
		shared_ptr<Model> model;
		if (!ResourceTracker::TryGetModel(id, model))
		{
			LoadModel(d3d, commandList, asset, mesh.primitives[i], model);
		}
		modelList.push_back(model);
	}
}

vector<shared_ptr<Model>> ModelLoader::LoadModelsFromGLTF(D3DClass* d3d, ID3D12GraphicsCommandList2* commandList, string modelName)
{
	string path = "Assets/Models/" + modelName;

	size_t dotIndex = modelName.find_last_of('.');
	if (dotIndex == string::npos)
		throw std::exception("Invalid model name");

	string modelNameExtensionless = modelName.substr(0, dotIndex);

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

	vector<shared_ptr<Model>> modelList;

	for (int i = 0; i < asset->scenes.size(); i++)
	{
		fastgltf::Scene& scene = asset->scenes[i];

		size_t nodeCount = scene.nodeIndices.size();
		for (size_t n = 0; n < nodeCount; n++)
		{
			int nodeIndex = scene.nodeIndices[n];
			fastgltf::Node& node = asset->nodes[nodeIndex];
			LoadModelsFromNode(d3d, commandList, asset, modelNameExtensionless, node, modelList);
		}
	}

	return modelList;
}