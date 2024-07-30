#include "pch.h"
#include "AssetFactory.h"
#include "ResourceManager.h"
#include "ResourceTracker.h"
#include "TextureLoader.h"

D3DClass* AssetFactory::ms_d3d;

void AssetFactory::Init(D3DClass* d3d)
{
	ms_d3d = d3d;
}

shared_ptr<Model> AssetFactory::CreateModel(string path, ID3D12GraphicsCommandList2* commandList)
{
	shared_ptr<Model> model;
	if (!ResourceTracker::TryGetModel(path, model))
	{
		model->Init(commandList, path);
	}
	return model;
}

shared_ptr<Texture> AssetFactory::CreateTexture(string path, ID3D12GraphicsCommandList2* commandList, bool flipUpsideDown, bool isNormalMap)
{
	shared_ptr<Texture> tex;
	if (!ResourceTracker::TryGetTexture(path, tex))
	{
		tex->Init(ms_d3d, commandList, path, flipUpsideDown, isNormalMap);
	}
	return tex;
}

shared_ptr<Texture> AssetFactory::CreateCubemapHDR(string path, ID3D12GraphicsCommandList2* commandList, bool flipUpsideDown)
{
	shared_ptr<Texture> tex;
	if (!ResourceTracker::TryGetTexture(path, tex))
	{
		tex->InitCubeMapHDR(ms_d3d, commandList, path, flipUpsideDown);
	}
	return tex;
}

shared_ptr<Texture> AssetFactory::CreateCubemap(vector<string> paths, ID3D12GraphicsCommandList2* commandList, bool flipUpsideDown)
{
	shared_ptr<Texture> tex;
	if (!ResourceTracker::TryGetTexture(paths, tex))
	{
		tex->InitCubeMap(ms_d3d, commandList, paths, flipUpsideDown);
	}
	return tex;
}

shared_ptr<Texture> AssetFactory::CreateIrradianceMap(Texture* cubemap, ID3D12GraphicsCommandList2* commandList)
{
	shared_ptr<Texture> tex = std::make_shared<Texture>();
	tex->InitCubeMapUAV_Empty(ms_d3d);
	TextureLoader::CreateIrradianceMap(ms_d3d, commandList, cubemap->GetResource(), tex->GetResource());
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
	CommandQueue* commandQueueCopy = nullptr;
	commandQueueCopy = ms_d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	if (!commandQueueCopy)
		throw std::exception("Command Queue Error");

	auto commandListCopy = commandQueueCopy->GetAvailableCommandList();

	shared_ptr<Model> model = AssetFactory::CreateModel(modelName, commandListCopy.Get());

	auto fenceValue = commandQueueCopy->ExecuteCommandList(commandListCopy);
	commandQueueCopy->WaitForFenceValue(fenceValue);

	CommandQueue* commandQueueDirect = nullptr;
	commandQueueDirect = ms_d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	if (!commandQueueDirect)
		throw std::exception("Command Queue Error");

	auto commandListDirect = commandQueueDirect->GetAvailableCommandList();

	vector<UINT> cbvSizes = { sizeof(MatricesCB) };

	RootParamInfo rootParamInfo;
	rootParamInfo.NumCBV_PerDraw = 1;
	rootParamInfo.ParamIndexCBV_PerDraw = 0;

	shared_ptr<RootSig> rootSig = std::make_shared<RootSig>();
	rootSig->InitDefaultSampler("Root Sig Cubes", rootParamInfo);

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
		material->AddCBVs(ms_d3d, commandListDirect.Get(), cbvSizes, false);
		ResourceTracker::AddMaterial(material);

		string id = modelName + " #" + std::to_string(i);
		auto go = batch->CreateGameObject(id, model, shader, material);

		float randX = (std::rand() / static_cast<float>(RAND_MAX)) * 2 - 1;
		float randY = (std::rand() / static_cast<float>(RAND_MAX)) * 2 - 1;
		float randZ = (std::rand() / static_cast<float>(RAND_MAX)) * 2 - 1;

		go->SetPosition(randX * range.x, randY * range.y, randZ * range.z);
	}
}