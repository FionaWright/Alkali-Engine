#include "pch.h"
#include "LoadManager.h"
#include <AlkaliGUIManager.h>

D3DClass* LoadManager::ms_d3dClass;
std::atomic<int> LoadManager::ms_numThreads;
std::atomic<bool> LoadManager::ms_loadingActive, LoadManager::ms_stopOnFlush;
queue<AsyncModelArgs> LoadManager::ms_modelQueue;
vector<std::unique_ptr<ThreadData>> LoadManager::ms_threadDatas;
queue<GPUWaitingList> LoadManager::ms_gpuWaitingLists;
std::unique_ptr<std::thread> LoadManager::ms_mainLoopThread;
CommandQueue* LoadManager::ms_cmdQueue;
std::mutex LoadManager::ms_mutexModelQueue, LoadManager::ms_mutexCpuWaitingLists, LoadManager::ms_mutexGpuWaitingLists;

void LoadManager::StartLoading(D3DClass* d3d, int numThreads)
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled)
		return;

	AlkaliGUIManager::LogAsyncMessage("====================================================");
	AlkaliGUIManager::LogAsyncMessage("Async Loading Begun");

	ms_d3dClass = d3d;
	ms_numThreads = numThreads;
	ms_loadingActive = true;
	ms_stopOnFlush = false;	

	ms_cmdQueue = d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);

	ms_threadDatas.clear();
	for (int i = 0; i < ms_numThreads; i++)
	{
		std::unique_ptr<ThreadData> threadData = std::make_unique<ThreadData>();
		threadData->CmdList = ms_cmdQueue->GetAvailableCommandList();
		ms_threadDatas.push_back(std::move(threadData));
	}

	ms_mainLoopThread = std::make_unique<std::thread>(LoadLoop);
}

void LoadManager::StopOnFlush()
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled)
		return;

	AlkaliGUIManager::LogAsyncMessage("Stop on flush enabled");
	ms_stopOnFlush = true;
}

void LoadManager::StopLoading()
{
	AlkaliGUIManager::LogAsyncMessage("Async loading ending");	

	for (int i = 0; i < ms_numThreads; i++)
	{
		if (ms_threadDatas[i]->pThread)
			ms_threadDatas[i]->pThread->join();

		delete ms_threadDatas[i]->pThread;
	}	

	AlkaliGUIManager::LogAsyncMessage("All threads exited");

	ms_loadingActive = false;	
}

void LoadManager::LoadLoop()
{
	while (ms_loadingActive)
	{
		if (ms_stopOnFlush && ms_modelQueue.size() == 0)
		{
			StopLoading();
			return;
		}

		for (int i = 0; i < ms_numThreads; i++)
		{
			if (ms_threadDatas[i]->Active)
				continue;

			if (ms_modelQueue.size() == 0)
				break;

			if (ms_threadDatas[i]->pThread)
			{
				ms_threadDatas[i]->pThread->join();

				std::thread newThread(LoadHighestPriority, i);
				ms_threadDatas[i]->pThread->swap(newThread);
				continue;
			}			

			ms_threadDatas[i]->pThread = new std::thread(LoadHighestPriority, i);
		}
	}
}

void LoadManager::LoadHighestPriority(int threadID)
{
	if (!SettingsManager::ms_DX12.DebugAsyncLogIgnoreInfo)
		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " begun");

	ms_threadDatas[threadID]->Active = true;

	std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
	if (ms_modelQueue.size() > 0)
	{
		AsyncModelArgs modelArgs = ms_modelQueue.front();
		ms_modelQueue.pop();	
		lock.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found model (" + modelArgs.FilePath + ")");

		LoadModel(modelArgs, threadID);
		ms_threadDatas[threadID]->Active = false;
		return;
	}
	lock.unlock();

	ms_threadDatas[threadID]->Active = false;
	if (!SettingsManager::ms_DX12.DebugAsyncLogIgnoreInfo)
		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found nothing");
}

void LoadManager::ExecuteCPUWaitingLists()
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled)
		return;

	if (!ms_loadingActive && ms_mainLoopThread)
	{
		ms_mainLoopThread->join();
		ms_mainLoopThread.reset();
		AlkaliGUIManager::LogAsyncMessage("Main load loop exited");
	}	

	if (!SettingsManager::ms_DX12.DebugAsyncLogIgnoreInfo)
		AlkaliGUIManager::LogAsyncMessage("CPU waiting list executing");

	std::unique_lock<std::mutex> lockCPU(ms_mutexCpuWaitingLists);
	std::unique_lock<std::mutex> lockGPU(ms_mutexGpuWaitingLists);

	for (int i = 0; i < ms_numThreads; i++)
	{
		if (ms_threadDatas[i]->CPU_WaitingList.size() == 0)
			continue;

		GPUWaitingList gpuWaitingList;

		gpuWaitingList.ModelList = ms_threadDatas[i]->CPU_WaitingList;
		ms_threadDatas[i]->CPU_WaitingList.clear();

		gpuWaitingList.FenceValue = ms_cmdQueue->ExecuteCommandList(ms_threadDatas[i]->CmdList);
		ms_threadDatas[i]->CmdList = ms_cmdQueue->GetAvailableCommandList();

		ms_gpuWaitingLists.push(gpuWaitingList);

		AlkaliGUIManager::LogAsyncMessage("CPU waiting list cleared, " + std::to_string(gpuWaitingList.ModelList.size()) + " models put on GPU waiting list with fence " + std::to_string(gpuWaitingList.FenceValue));
	}

	while (ms_gpuWaitingLists.size() > 0)
	{
		if (!ms_cmdQueue->IsFenceComplete(ms_gpuWaitingLists.front().FenceValue))
			return;

		AlkaliGUIManager::LogAsyncMessage("GPU waiting list complete with fence " + std::to_string(ms_gpuWaitingLists.front().FenceValue));

		for (auto mdl : ms_gpuWaitingLists.front().ModelList)
			mdl->MarkLoaded();

		ms_gpuWaitingLists.pop();
	}
}

bool LoadManager::TryPushModel(Model* pModel, string filePath)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Model tried to be pushed to queue: " + filePath + " but async wasn't enabled");
		return false;
	}

	AlkaliGUIManager::LogAsyncMessage("Model pushed to queue: " + filePath);

	AsyncModelArgs modelArg;
	modelArg.pModel = pModel;
	modelArg.FilePath = filePath;

	std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
	ms_modelQueue.push(modelArg);
	return true;
}

bool LoadManager::TryPushModel(Model* pModel, Asset asset, int meshIndex, int primitiveIndex)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("GLTF Model tried to be pushed to queue but async wasn't enabled");
		return false;
	}

	AlkaliGUIManager::LogAsyncMessage("GLTF Model pushed to queue");

	AsyncModelArgs modelArg;
	modelArg.pModel = pModel;
	modelArg.Asset = asset;
	modelArg.MeshIndex = meshIndex;
	modelArg.PrimitiveIndex = primitiveIndex;

	std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
	ms_modelQueue.push(modelArg);
	return true;
}

void LoadManager::LoadModel(AsyncModelArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.DebugModelLoadDelayMillis > 0)
		Sleep(SettingsManager::ms_DX12.DebugModelLoadDelayMillis);

	if (args.FilePath != "")
	{
		bool success = args.pModel->Init(ms_threadDatas[threadID]->CmdList.Get(), args.FilePath);
		if (!success)
		{
			AlkaliGUIManager::LogErrorMessage("Failed to load model [async] (path=\"" + args.FilePath + "\")");
			return;
		}
	}		
	else if (args.Asset)
	{
		auto& primitive = args.Asset->get().meshes[args.MeshIndex].primitives[args.PrimitiveIndex];
		ModelLoaderGLTF::LoadModel(ms_d3dClass, ms_threadDatas[threadID]->CmdList.Get(), args.Asset, primitive, args.pModel);
	}		
	else
		throw std::exception();

	std::unique_lock<std::mutex> lock(ms_mutexCpuWaitingLists);

	ms_threadDatas[threadID]->CPU_WaitingList.push_back(args.pModel);
}
