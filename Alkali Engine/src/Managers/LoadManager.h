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
	Model* pModel;
	string FilePath;

	// GLTF Args:
	Asset Asset;
	int MeshIndex, PrimitiveIndex;
};

struct ThreadData
{
	std::thread* pThread = nullptr;
	std::atomic<bool> Active = false;	
	ComPtr<ID3D12GraphicsCommandList2> CmdList = nullptr;

	vector<Model*> CPU_WaitingList;

	ThreadData() {}
};

struct GPUWaitingList
{
	uint64_t FenceValue = 0;
	vector<Model*> ModelList;
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
	static bool TryPushModel(Model* pModel, Asset asset, int meshIndex, int primitiveIndex);

private:
	static void LoadLoop();
	static void LoadHighestPriority(int threadID);
	static void LoadModel(AsyncModelArgs args, int threadID);

	static D3DClass* ms_d3dClass;
	static CommandQueue* ms_cmdQueue;
	static std::atomic<int> ms_numThreads;
	static std::atomic<bool> ms_loadingActive, ms_stopOnFlush, ms_fullShutdownActive;
	static vector<std::unique_ptr<ThreadData>> ms_threadDatas;
	static queue<GPUWaitingList> ms_gpuWaitingLists;
	static std::unique_ptr<std::thread> ms_mainLoopThread;

	static queue<AsyncModelArgs> ms_modelQueue;
	static std::mutex ms_mutexModelQueue, ms_mutexCpuWaitingLists, ms_mutexGpuWaitingLists;
};

