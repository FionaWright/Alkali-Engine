#pragma once

#include "pch.h"
#include "Model.h"
#include "GameObject.h"
#include "Batch.h"
#include "ModelLoaderGLTF.h"

#include <thread>
#include <mutex>

using std::shared_ptr;
using std::string;
using std::vector;
using std::queue;

struct AsyncModelArgs
{
	Model* pModel = nullptr;
	string FilePath = "";

	// Alt GLTF Args:
	Asset Asset; // Use std::variant?
	size_t MeshIndex = -1, PrimitiveIndex = -1;
};

struct AsyncTexArgs
{
	Texture* pTexture = nullptr;
	string FilePath = "";
	bool FlipUpsideDown = false;
	bool IsNormalMap = false;
	bool DisableMips = false;
};

struct AsyncTexCubemapArgs
{
	Texture* pCubemap = nullptr;
	Texture* pIrradiance = nullptr;

	string FilePath = "";
	vector<string> FilePaths;
	bool FlipUpsideDown = false;
};

struct AsyncShaderArgs
{
	Shader* pShader;
	ShaderArgs Args;
};

struct ThreadData
{
	ComPtr<ID3D12GraphicsCommandList2> CmdListCopy = nullptr;
	ComPtr<ID3D12GraphicsCommandList2> CmdListCompute = nullptr;

	vector<Model*> CPU_WaitingListModel;
	vector<Texture*> CPU_WaitingListTexture;

	ThreadData() {}
};

struct GPUWaitingList
{
	bool IsCOPY = true;

	uint64_t FenceValue = 0;
	vector<Model*> ModelList;
	vector<Texture*> TextureList;
};

class LoadManager
{
public:    
	static void StartLoading(D3DClass* d3d, int numThreads);
	static void EnableStopOnFlush();
	static void StopLoading();
	static void FullShutdown();

	static void ExecuteCPUWaitingLists();
	static bool TryPushModel(Model* pModel, string filePath);	
	static bool TryPushModel(Model* pModel, Asset asset, size_t meshIndex, size_t primitiveIndex);

	static bool TryPushTex(Texture* pTex, string filePath, bool flipUpsideDown = false, bool isNormalMap = false, bool disableMips = false);
	static bool TryPushCubemap(Texture* pTex, string filePath, Texture* pIrradiance = nullptr, bool flipUpsideDown = false);
	static bool TryPushCubemap(Texture* pTex, vector<string> filePaths, Texture* pIrradiance = nullptr, bool flipUpsideDown = false);
	static bool TryPushShader(Shader* pShader, const ShaderArgs& shaderArgs);

private:
	static void LoadLoop(int threadID);
	static void LoadHighestPriority(int threadID);
	static void LoadModel(AsyncModelArgs args, int threadID);
	static void LoadTex(AsyncTexArgs args, int threadID);
	static void LoadTexCubemap(AsyncTexCubemapArgs args, int threadID);
	static void LoadShader(AsyncShaderArgs args, int threadID);
	static bool AllQueuesFlushed();

	static D3DClass* ms_d3dClass;
	static CommandQueue* ms_copyQueue, *ms_computeQueue;

	static int ms_numThreads;
	static std::atomic<bool> ms_loadingActive, ms_stopOnFlush, ms_fullShutdownActive;

	static vector<std::unique_ptr<ThreadData>> ms_threadDatas;
	static vector<std::jthread> ms_loadThreads;
	static queue<GPUWaitingList> ms_gpuWaitingLists;

	static queue<AsyncModelArgs> ms_modelQueue;
	static queue<AsyncTexArgs> ms_texQueue;
	static queue<AsyncTexCubemapArgs> ms_texCubemapQueue;
	static queue<AsyncShaderArgs> ms_shaderQueue;
	static std::mutex ms_mutexModelQueue, ms_mutexTexQueue, ms_mutexTexCubemapQueue, ms_mutexShaderQueue, ms_notifyMutex;
	static std::mutex ms_mutexThreadDatas[8];
	static std::condition_variable ms_conditionVariable;
};

