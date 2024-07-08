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

shared_ptr<Shader> AssetFactory::CreateShader(wstring vs, wstring ps, const vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout, RootSig* rootSig, bool preCompiled, bool cullOff, bool disableDSV, bool disableDSVWrite, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
{
	shared_ptr<Shader> shader;
	wstring id = vs + L" - " + ps;
	if (cullOff)
		id += L" --CullOff";

	if (!ResourceTracker::TryGetShader(id, shader))
	{
		if (preCompiled)
			shader->InitPreCompiled(vs, ps, inputLayout, rootSig->GetRootSigResource(), ms_d3d->GetDevice(), cullOff, disableDSV, disableDSVWrite, topology);
		else
			shader->Init(vs, ps, inputLayout, rootSig->GetRootSigResource(), ms_d3d->GetDevice(), cullOff, disableDSV, disableDSVWrite, topology);
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
