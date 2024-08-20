#include "pch.h"
#include "AssetFactory.h"
#include "ResourceManager.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"
#include <AlkaliGUIManager.h>
#include "LoadManager.h"

D3DClass* AssetFactory::ms_d3d;

void AssetFactory::Init(D3DClass* d3d)
{
	ms_d3d = d3d;
}

shared_ptr<Model> AssetFactory::CreateModel(string path, ID3D12GraphicsCommandList2* cmdList)
{
	if (SettingsManager::ms_DX12.DebugCubeModelOnly)
		path = "Cube.model";

	shared_ptr<Model> model;
	if (!ResourceTracker::TryGetModel(path, model))
	{
		if (SettingsManager::ms_DX12.AsyncLoadingEnabled && LoadManager::TryPushModel(model.get(), path))
			return model;

		if (!cmdList)
			throw std::exception("Attempted to load model synchronously without command list");

		bool success = model->Init(cmdList, path);
		if (!success)
			AlkaliGUIManager::LogErrorMessage("Failed to load model (path=\"" + path + "\")");
		else
			model->MarkLoaded();
	}

	return model;
}

shared_ptr<Texture> AssetFactory::CreateTexture(string path, ID3D12GraphicsCommandList2* cmdList, bool flipUpsideDown, bool isNormalMap, bool disableMips)
{
	if (SettingsManager::ms_DX12.DebugWhiteTextureOnly)
		path = "WhitePOT.png";

	shared_ptr<Texture> tex;
	if (!ResourceTracker::TryGetTexture(path, tex))
	{
		if (SettingsManager::ms_DX12.AsyncLoadingEnabled && LoadManager::TryPushTex(tex.get(), path, flipUpsideDown, isNormalMap, disableMips))
			return tex;

		if (!cmdList)
			throw std::exception("Attempted to load model synchronously without command list");

		tex->Init(ms_d3d, cmdList, path, flipUpsideDown, isNormalMap, disableMips);
		tex->MarkLoaded();
	}
	return tex;
}

shared_ptr<Texture> AssetFactory::CreateCubemapHDR(string path, ID3D12GraphicsCommandList2* cmdList, bool flipUpsideDown)
{
	shared_ptr<Texture> tex;
	if (!ResourceTracker::TryGetTexture(path, tex))
	{
		tex->InitCubeMapHDR(ms_d3d, cmdList, path, flipUpsideDown);
		tex->MarkLoaded();
	}
	return tex;
}

shared_ptr<Texture> AssetFactory::CreateCubemap(vector<string> paths, ID3D12GraphicsCommandList2* cmdList, bool flipUpsideDown)
{
	shared_ptr<Texture> tex;
	if (!ResourceTracker::TryGetTexture(paths, tex))
	{
		tex->InitCubeMap(ms_d3d, cmdList, paths, flipUpsideDown);
		tex->MarkLoaded();
	}
	return tex;
}

shared_ptr<Texture> AssetFactory::CreateIrradianceMap(Texture* cubemap, ID3D12GraphicsCommandList2* cmdList)
{
	shared_ptr<Texture> tex = std::make_shared<Texture>();
	tex->InitCubeMapUAV_Empty(ms_d3d);	
	TextureLoader::CreateIrradianceMap(ms_d3d, cmdList, cubemap->GetResource(), tex->GetResource());
	tex->MarkLoaded();
	return tex;
}

shared_ptr<Shader> AssetFactory::CreateShader(const ShaderArgs& args, bool precompiled)
{
	shared_ptr<Shader> shader;
	wstring id = args.vs + L" - " + args.ps;
	if (args.CullNone)
		id += L" --CullOff";

	if (!ResourceTracker::TryGetShader(id, shader))
	{
		if (precompiled)
			shader->InitPreCompiled(ms_d3d->GetDevice(), args);
		else
			shader->Init(ms_d3d->GetDevice(), args);
	}
	return shader;
}

shared_ptr<Batch> AssetFactory::CreateBatch(shared_ptr<RootSig> rootSig)
{
	string id = "Batch for " + rootSig->GetName();

	shared_ptr<Batch> batch;
	if (!ResourceTracker::TryGetBatch(id, batch))
	{
		batch->Init(id, rootSig);
	}
	return batch;
}

shared_ptr<Material> AssetFactory::CreateMaterial()
{
	auto mat = std::make_shared<Material>();
	ResourceTracker::AddMaterial(mat);
	return mat;
}

void AssetFactory::InstantiateObjects(string modelName, int count, const XMFLOAT3& range)
{
	CommandQueue* cmdQueueCopy = nullptr;
	cmdQueueCopy = ms_d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	if (!cmdQueueCopy)
		throw std::exception("Command Queue Error");

	auto cmdListCopy = cmdQueueCopy->GetAvailableCommandList();

	shared_ptr<Model> model = AssetFactory::CreateModel(modelName, cmdListCopy.Get());

	auto fenceValue = cmdQueueCopy->ExecuteCommandList(cmdListCopy);
	cmdQueueCopy->WaitForFenceValue(fenceValue);

	CommandQueue* cmdQueueDirect = nullptr;
	cmdQueueDirect = ms_d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!cmdQueueDirect)
		throw std::exception("Command Queue Error");

	auto cmdListDirect = cmdQueueDirect->GetAvailableCommandList();

	vector<UINT> cbvSizes = { sizeof(MatricesCB) };

	RootParamInfo rootParamInfo;
	rootParamInfo.NumCBV_PerDraw = 1;
	rootParamInfo.ParamIndexCBV_PerDraw = 0;

	shared_ptr<RootSig> rootSig = std::make_shared<RootSig>();
	rootSig->Init("Root Sig Cubes", rootParamInfo, &SettingsManager::ms_DX12.DefaultSamplerDesc, 1);

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	ShaderArgs args = { L"Shrimple_VS.cso", L"Shrimple_PS.cso", inputLayout, rootSig->GetRootSigResource() };

	shared_ptr<Shader> shader = AssetFactory::CreateShader(args, true);

	shared_ptr<Batch> batch = AssetFactory::CreateBatch(rootSig);

	for (int i = 0; i < count; i++)
	{
		shared_ptr<Material> material = std::make_shared<Material>();
		material->AddCBVs(ms_d3d, cmdListDirect.Get(), cbvSizes, false);
		ResourceTracker::AddMaterial(material);

		string id = modelName + " #" + std::to_string(i);
		auto go = batch->CreateGameObject(id, model, shader, material);

		float randX = Rand01() * 2 - 1;
		float randY = Rand01() * 2 - 1;
		float randZ = Rand01() * 2 - 1;

		go->SetPosition(randX * range.x, randY * range.y, randZ * range.z);
	}
}