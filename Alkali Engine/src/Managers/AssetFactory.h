#pragma once

#include "pch.h"
#include "Model.h"
#include "GameObject.h"
#include "Batch.h"

using std::shared_ptr;
using std::string;
using std::vector;

class AssetFactory
{
public:
    static void Init(D3DClass* d3d);

    static shared_ptr<Model> CreateModel(string path, ID3D12GraphicsCommandList2* cmdList =  nullptr);
    static shared_ptr<Texture> CreateTexture(string path, ID3D12GraphicsCommandList2* cmdList, bool flipUpsideDown = false, bool isNormalMap = false, bool disableMips = false);
    static shared_ptr<Texture> CreateCubemapHDR(string path, ID3D12GraphicsCommandList2* cmdList, bool flipUpsideDown = false);
    static shared_ptr<Texture> CreateCubemap(vector<string> paths, ID3D12GraphicsCommandList2* cmdList, bool flipUpsideDown = false);
    static shared_ptr<Texture> CreateIrradianceMap(Texture* cubemap, ID3D12GraphicsCommandList2* cmdList);
    static shared_ptr<Shader> CreateShader(const ShaderArgs& args, bool precompiled = false);
    static shared_ptr<Batch> CreateBatch(shared_ptr<RootSig> rootSig);
    static shared_ptr<Material> CreateMaterial();

    static void InstantiateObjects(string modelName, int count, const XMFLOAT3& range);

private:
    static D3DClass* ms_d3d;
};

