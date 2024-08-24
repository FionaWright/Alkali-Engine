#include "pch.h"
#include "ResourceTracker.h"

unordered_map<string, shared_ptr<Texture>> ResourceTracker::ms_textureMap;
unordered_map<string, shared_ptr<Model>> ResourceTracker::ms_modelMap;
unordered_map<string, shared_ptr<Shader>> ResourceTracker::ms_shaderMap;
unordered_map<string, shared_ptr<Batch>> ResourceTracker::ms_batchMap;
vector<shared_ptr<Material>> ResourceTracker::ms_matList;

void ResourceTracker::AddTexture(string filePath, shared_ptr<Texture> tex)
{
	if (ms_textureMap.contains(filePath))
		return;

	ms_textureMap.emplace(filePath, tex);
}

bool ResourceTracker::TryGetTexture(string filePath, shared_ptr<Texture>& tex)
{
	if (!ms_textureMap.contains(filePath))
	{
		tex = std::make_shared<Texture>();
		AddTexture(filePath, tex);
		return false;
	}		

	tex = ms_textureMap.at(filePath);
	return true;
}

bool ResourceTracker::TryGetTexture(vector<string>& filepaths, shared_ptr<Texture>& tex)
{
	string id = "";
	for (int i = 0; i < filepaths.size(); i++)
	{
		if (i != 0)
			id += "~";
		id += filepaths[i];
	}

	if (!ms_textureMap.contains(id))
	{
		tex = std::make_shared<Texture>();
		AddTexture(id, tex);
		return false;
	}

	tex = ms_textureMap.at(id);
	return true;
}

unordered_map<string, shared_ptr<Texture>>& ResourceTracker::GetTextures()
{
	return ms_textureMap;
}

void ResourceTracker::ClearTexList()
{
	ms_textureMap.clear();
}

void ResourceTracker::AddModel(string filePath, shared_ptr<Model> model)
{
	if (ms_modelMap.contains(filePath))
		return;

	ms_modelMap.emplace(filePath, model);
}

bool ResourceTracker::TryGetModel(string filePath, shared_ptr<Model>& model)
{
	if (SettingsManager::ms_DX12.DebugCubeModelOnly)
		filePath = "Cube.model";

	if (!ms_modelMap.contains(filePath))
	{
		model = std::make_shared<Model>();
		AddModel(filePath, model);
		return false;
	}

	model = ms_modelMap.at(filePath);
	return true;
}

unordered_map<string, shared_ptr<Model>>& ResourceTracker::GetModels()
{
	return ms_modelMap;
}

void ResourceTracker::AddShader(string filePath, shared_ptr<Shader> shader)
{
	if (ms_shaderMap.contains(filePath))
		return;

	ms_shaderMap.emplace(filePath, shader);
}

bool ResourceTracker::TryGetShader(string filePath, shared_ptr<Shader>& shader)
{
	if (!ms_shaderMap.contains(filePath))
	{
		shader = std::make_shared<Shader>();
		AddShader(filePath, shader);
		return false;
	}

	shader = ms_shaderMap.at(filePath);
	return true;
}

bool ResourceTracker::TryGetShader(wstring filePath, shared_ptr<Shader>& shader) 
{
	string filePathStr = wstringToString(filePath);
	return TryGetShader(filePathStr, shader);
}

unordered_map<string, shared_ptr<Shader>>& ResourceTracker::GetShaders()
{
	return ms_shaderMap;
}

void ResourceTracker::RecompileAllShaders(ID3D12Device2* device)
{
	for (auto& it : ms_shaderMap)
	{
		if (!it.second->IsInitialised())
			continue;

		it.second->Compile(device);
	}
}

void ResourceTracker::ClearShaderList()
{
	ms_shaderMap.clear();
}

void ResourceTracker::AddBatch(string filePath, shared_ptr<Batch> batch)
{
	if (ms_batchMap.contains(filePath))
		return;

	ms_batchMap.emplace(filePath, batch);
}

bool ResourceTracker::TryGetBatch(string filePath, shared_ptr<Batch>& batch)
{
	if (!ms_batchMap.contains(filePath))
	{
		batch = std::make_shared<Batch>();
		AddBatch(filePath, batch);
		return false;
	}

	batch = ms_batchMap.at(filePath);
	return true;
}

unordered_map<string, shared_ptr<Batch>>& ResourceTracker::GetBatches()
{
	return ms_batchMap;
}

void ResourceTracker::ClearBatchList()
{
	ms_batchMap.clear();
}

void ResourceTracker::AddMaterial(shared_ptr<Material> mat)
{
	ms_matList.push_back(mat);
}

vector<shared_ptr<Material>>& ResourceTracker::GetMaterials()
{
	return ms_matList;
}

void ResourceTracker::ClearMatList()
{
	ms_matList.clear();
}

void ResourceTracker::ReleaseAll()
{
	ms_textureMap.clear();
	ms_modelMap.clear();
	ms_shaderMap.clear();
	ms_batchMap.clear();
	ms_matList.clear();
}
