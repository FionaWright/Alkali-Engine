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

    static shared_ptr<Model> CreateModel(string path, ID3D12GraphicsCommandList2* commandList);
    static shared_ptr<Texture> CreateTexture(string path, ID3D12GraphicsCommandList2* commandList, bool flipUpsideDown = false, bool isNormalMap = false);
    static shared_ptr<Texture> CreateCubemapHDR(string path, ID3D12GraphicsCommandList2* commandList, bool flipUpsideDown = false);
    static shared_ptr<Texture> CreateCubemap(vector<string> paths, ID3D12GraphicsCommandList2* commandList, bool flipUpsideDown = false);
    static shared_ptr<Texture> CreateIrradianceMap(Texture* cubemap, ID3D12GraphicsCommandList2* commandList);
    static shared_ptr<Shader> CreateShader(wstring vs, wstring ps, const vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout, RootSig* rootSig, bool preCompiled = false, bool cullOff = false, bool disableDSV = false, bool disableDSVWrite = false, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    static shared_ptr<Batch> CreateBatch(shared_ptr<RootSig> rootSig);

    static void InstantiateObjects(string modelName, int count);

private:
    static D3DClass* ms_d3d;
};

