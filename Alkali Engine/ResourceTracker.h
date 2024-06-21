#pragma once

#include "pch.h"
#include <unordered_map>
#include "Texture.h"
#include "Model.h"
#include "GameObject.h"
#include "Batch.h"
#include <vector>

using std::string;
using std::unordered_map;
using std::vector;

class ResourceTracker
{
public:
	static void AddTexture(string filePath, shared_ptr<Texture> tex);
	static bool TryGetTexture(string filePath, shared_ptr<Texture>& tex);
	static unordered_map<string, shared_ptr<Texture>>& GetTextures();

	static void AddModel(string filePath, shared_ptr<Model> model);
	static bool TryGetModel(string filePath, shared_ptr<Model>& model);
	static unordered_map<string, shared_ptr<Model>>& GetModels();

	static void AddShader(string filePath, shared_ptr<Shader> shader);
	static bool TryGetShader(string filePath, shared_ptr<Shader>& shader);
	static unordered_map<string, shared_ptr<Shader>>& GetShaders();

	static void AddMaterial(string filePath, shared_ptr<Material> material);
	static bool TryGetMaterial(string filePath, shared_ptr<Material>& material);
	static unordered_map<string, shared_ptr<Material>>& GetMaterials();

	static void AddBatch(string filePath, shared_ptr<Batch> batch);
	static bool TryGetBatch(string filePath, shared_ptr<Batch>& batch);
	static unordered_map<string, shared_ptr<Batch>>& GetBatches();
	static void ClearBatchList();

	static void ReleaseAll();

private:
	static unordered_map<string, shared_ptr<Texture>> ms_textureMap;
	static unordered_map<string, shared_ptr<Model>> ms_modelMap;
	static unordered_map<string, shared_ptr<Shader>> ms_shaderMap;
	static unordered_map<string, shared_ptr<Material>> ms_materialMap;
	static unordered_map<string, shared_ptr<Batch>> ms_batchMap;
};

