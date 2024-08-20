#include "pch.h"
#include "LoadManager.h"
#include <AlkaliGUIManager.h>

D3DClass* LoadManager::ms_d3dClass;
CommandQueue* LoadManager::ms_cmdQueue;

std::atomic<int> LoadManager::ms_numThreads;
std::atomic<bool> LoadManager::ms_loadingActive, LoadManager::ms_stopOnFlush, LoadManager::ms_fullShutdownActive;

vector<std::unique_ptr<ThreadData>> LoadManager::ms_threadDatas;
queue<GPUWaitingList> LoadManager::ms_gpuWaitingLists;
std::unique_ptr<std::thread> LoadManager::ms_mainLoopThread;

queue<AsyncModelArgs> LoadManager::ms_modelQueue;
queue<AsyncTexArgs> LoadManager::ms_texQueue;
std::mutex LoadManager::ms_mutexModelQueue, LoadManager::ms_mutexTexQueue, LoadManager::ms_mutexCpuWaitingLists, LoadManager::ms_mutexGpuWaitingLists;

void LoadManager::StartLoading(D3DClass* d3d, int numThreads)
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled || ms_fullShutdownActive)
		return;

	AlkaliGUIManager::LogAsyncMessage("====================================================");

	bool prevSceneThreadsActive = ms_loadingActive || ms_mainLoopThread;
	if (prevSceneThreadsActive)
	{
		ms_loadingActive = false;
		if (ms_mainLoopThread)
			ms_mainLoopThread->join();

		StopLoading();
		AlkaliGUIManager::LogAsyncMessage("Closed existing threads");
	}
	
	AlkaliGUIManager::LogAsyncMessage("Async Loading Begun");

	ms_d3dClass = d3d;
	ms_numThreads = numThreads;
	ms_loadingActive = true;
	ms_stopOnFlush = false;	

	ms_cmdQueue = d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	ms_threadDatas.clear();
	for (int i = 0; i < ms_numThreads; i++)
	{
		std::unique_ptr<ThreadData> threadData = std::make_unique<ThreadData>();
		threadData->CmdList = ms_cmdQueue->GetAvailableCommandList();
		ms_threadDatas.push_back(std::move(threadData)); // std::move is required here probably because of the std::atomic<bool> inside
	}

	ms_mainLoopThread = std::make_unique<std::thread>(LoadLoop);
}

void LoadManager::EnableStopOnFlush()
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled || ms_fullShutdownActive)
		return;

	AlkaliGUIManager::LogAsyncMessage("Stop on flush enabled");
	ms_stopOnFlush = true;
}

void LoadManager::StopLoading()
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled || ms_fullShutdownActive)
		return;

	AlkaliGUIManager::LogAsyncMessage("Async loading ending");	

	ms_loadingActive = false;

	for (int i = 0; i < ms_numThreads; i++)
	{
		if (!ms_threadDatas[i]->pThread)
			continue;
		
		ms_threadDatas[i]->pThread->join();
		delete ms_threadDatas[i]->pThread;
		ms_threadDatas[i]->pThread = nullptr;
	}	

	AlkaliGUIManager::LogAsyncMessage("All threads exited");	
}

void LoadManager::FullShutdown()
{
	if (ms_fullShutdownActive)
		return;

	ms_fullShutdownActive = true;
	ms_stopOnFlush = false;
	ms_loadingActive = false;
	if (ms_mainLoopThread)
	{
		ms_mainLoopThread->join();
		ms_mainLoopThread.reset();
	}	

	for (int i = 0; i < ms_numThreads; i++)
	{
		if (!ms_threadDatas[i]->pThread)
			continue;
		
		if (ms_threadDatas[i]->pThread->joinable())
			ms_threadDatas[i]->pThread->join();
		delete ms_threadDatas[i]->pThread;
		ms_threadDatas[i]->pThread = nullptr;
	}

	ms_threadDatas.clear();
	AlkaliGUIManager::LogAsyncMessage("Full Shutdown Completed");
}

void LoadManager::LoadLoop()
{
	while (ms_loadingActive)
	{
		bool modelQueueFlushed = ms_modelQueue.size() == 0;
		bool texQueueFlushed = ms_texQueue.size() == 0;
		if (ms_stopOnFlush && modelQueueFlushed && texQueueFlushed)
		{
			StopLoading();
			return;
		}

		for (int i = 0; i < ms_numThreads; i++)
		{
			if (!ms_loadingActive || ms_threadDatas[i]->Active)
				continue;

			bool modelQueueFlushed = ms_modelQueue.size() == 0;
			bool texQueueFlushed = ms_texQueue.size() == 0;
			if (modelQueueFlushed && texQueueFlushed)
				break;

			bool canReuseThread = ms_threadDatas[i]->pThread;
			if (canReuseThread)
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

	std::unique_lock<std::mutex> lockModel(ms_mutexModelQueue);
	if (ms_modelQueue.size() > 0)
	{
		AsyncModelArgs modelArgs = ms_modelQueue.front();
		ms_modelQueue.pop();	
		lockModel.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found model (" + modelArgs.FilePath + ")");

		LoadModel(modelArgs, threadID);
		ms_threadDatas[threadID]->Active = false;
		return;
	}
	lockModel.unlock();

	std::unique_lock<std::mutex> lockTex(ms_mutexTexQueue);
	if (ms_texQueue.size() > 0)
	{
		AsyncTexArgs texArgs = ms_texQueue.front();
		ms_texQueue.pop();
		lockTex.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found tex (" + texArgs.FilePath + ")");

		LoadTex(texArgs, threadID);
		ms_threadDatas[threadID]->Active = false;
		return;
	}
	lockTex.unlock();

	// TODO: Shaders

	ms_threadDatas[threadID]->Active = false;
	if (!SettingsManager::ms_DX12.DebugAsyncLogIgnoreInfo)
		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found nothing");
}

void LoadManager::ExecuteCPUWaitingLists()
{
	if (!SettingsManager::ms_DX12.AsyncLoadingEnabled)
		return;

	bool mainLoopNeedsRejoining = !ms_loadingActive && ms_mainLoopThread;
	if (mainLoopNeedsRejoining)
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
		bool emptyModelList = ms_threadDatas[i]->CPU_WaitingListModel.size() == 0;
		bool emptyTexList = ms_threadDatas[i]->CPU_WaitingListTexture.size() == 0;
		if (emptyModelList && emptyTexList)
			continue;

		GPUWaitingList gpuWaitingList;
		gpuWaitingList.ModelList = ms_threadDatas[i]->CPU_WaitingListModel;		
		gpuWaitingList.TextureList = ms_threadDatas[i]->CPU_WaitingListTexture;		
		gpuWaitingList.FenceValue = ms_cmdQueue->ExecuteCommandList(ms_threadDatas[i]->CmdList);
		ms_gpuWaitingLists.push(gpuWaitingList);

		ms_threadDatas[i]->CPU_WaitingListModel.clear();
		ms_threadDatas[i]->CPU_WaitingListTexture.clear();
		ms_threadDatas[i]->CmdList = ms_cmdQueue->GetAvailableCommandList();		

		string strModelCount = std::to_string(gpuWaitingList.ModelList.size());
		string strFenceVal = std::to_string(gpuWaitingList.FenceValue);
		AlkaliGUIManager::LogAsyncMessage("CPU waiting list cleared, " + strModelCount + " models put on GPU waiting list with fence " + strFenceVal);
	}
	lockCPU.unlock();

	while (ms_gpuWaitingLists.size() > 0)
	{
		// Lowest fence value will always be at the front
		if (!ms_cmdQueue->IsFenceComplete(ms_gpuWaitingLists.front().FenceValue))
			return;

		AlkaliGUIManager::LogAsyncMessage("GPU waiting list complete with fence " + std::to_string(ms_gpuWaitingLists.front().FenceValue));

		for (auto mdl : ms_gpuWaitingLists.front().ModelList)
			mdl->MarkLoaded();

		for (auto tex : ms_gpuWaitingLists.front().TextureList)
			tex->MarkLoaded();

		ms_gpuWaitingLists.pop();
	}
}

bool LoadManager::TryPushModel(Model* pModel, string filePath)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Model tried to be pushed to queue: " + filePath + " but async wasn't enabled");
		return false; // Loads syncronously
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
		return false; // Loads syncronously
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

bool LoadManager::TryPushTex(Texture* pTex, string filePath, bool flipUpsideDown, bool isNormalMap, bool disableMips)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Texture tried to be pushed to queue: " + filePath + " but async wasn't enabled");
		return false; // Loads syncronously
	}

	AlkaliGUIManager::LogAsyncMessage("Texture pushed to queue: " + filePath);

	AsyncTexArgs texArgs;
	texArgs.pTexture = pTex;
	texArgs.FilePath = filePath;
	texArgs.FlipUpsideDown = flipUpsideDown;
	texArgs.IsNormalMap = isNormalMap;
	texArgs.DisableMips = disableMips;

	std::unique_lock<std::mutex> lock(ms_mutexTexQueue);
	ms_texQueue.push(texArgs);
	return true;
}

void LoadManager::LoadModel(AsyncModelArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.DebugModelLoadDelayMillis > 0)
		Sleep(SettingsManager::ms_DX12.DebugModelLoadDelayMillis);

	if (!args.pModel || ms_fullShutdownActive)
		return;

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
	ms_threadDatas[threadID]->CPU_WaitingListModel.push_back(args.pModel);
}

void LoadManager::LoadTex(AsyncTexArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.DebugModelLoadDelayMillis > 0)
		Sleep(SettingsManager::ms_DX12.DebugModelLoadDelayMillis);

	if (!args.pTexture || ms_fullShutdownActive)
		return;

	if (args.FilePath == "")
		throw std::exception();

	auto cmdList = ms_threadDatas[threadID]->CmdList.Get();
	args.pTexture->Init(ms_d3dClass, cmdList, args.FilePath, args.FlipUpsideDown, args.IsNormalMap, args.DisableMips);

	std::unique_lock<std::mutex> lock(ms_mutexCpuWaitingLists);
	ms_threadDatas[threadID]->CPU_WaitingListTexture.push_back(args.pTexture);
}
