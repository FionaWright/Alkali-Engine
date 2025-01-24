#include "pch.h"
#include "LoadManager.h"
#include <AlkaliGUIManager.h>
#include <TextureLoader.h>

D3DClass* LoadManager::ms_d3dClass;
CommandQueue* LoadManager::ms_copyQueue, * LoadManager::ms_computeQueue;

int LoadManager::ms_numThreads;
std::atomic<bool> LoadManager::ms_loadingActive, LoadManager::ms_stopOnFlush, LoadManager::ms_fullShutdownActive;

vector<std::unique_ptr<ThreadData>> LoadManager::ms_threadDatas;
queue<GPUWaitingList> LoadManager::ms_gpuWaitingLists;
vector<std::jthread> LoadManager::ms_loadThreads;

queue<AsyncModelArgs> LoadManager::ms_modelQueue;
queue<AsyncTexArgs> LoadManager::ms_texQueue;
queue<AsyncTexCubemapArgs> LoadManager::ms_texCubemapQueue;
queue<AsyncShaderArgs> LoadManager::ms_shaderQueue;
std::mutex LoadManager::ms_mutexModelQueue, LoadManager::ms_mutexTexQueue, LoadManager::ms_mutexTexCubemapQueue, LoadManager::ms_mutexShaderQueue, LoadManager::ms_notifyMutex;
std::mutex LoadManager::ms_mutexThreadDatas[8];
std::condition_variable LoadManager::ms_conditionVariable;

void LoadManager::StartLoading(D3DClass* d3d, int numThreads)
{
	if (!SettingsManager::ms_DX12.Async.Enabled || ms_fullShutdownActive)
		return;

	AlkaliGUIManager::LogAsyncMessage("========================START=========================");

	bool prevSceneThreadsActive = ms_loadingActive || ms_loadThreads.size() > 0;
	if (prevSceneThreadsActive)
	{
		StopLoading();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Closed existing threads");
	}
	
	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Async Loading Begun");

	ms_d3dClass = d3d;
	ms_numThreads = numThreads;
	ms_loadingActive = true;
	ms_stopOnFlush = false;	

	if (ms_numThreads > _countof(ms_mutexThreadDatas))
		throw std::exception();

	ms_copyQueue = d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	ms_computeQueue = d3d->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);

	ms_threadDatas.clear();
	ms_loadThreads.clear();
	for (int i = 0; i < ms_numThreads; i++)
	{
		std::unique_ptr<ThreadData> threadData = std::make_unique<ThreadData>();
		threadData->CmdListCopy = ms_copyQueue->GetAvailableCommandList();
		threadData->CmdListCompute = ms_computeQueue->GetAvailableCommandList();
		ms_threadDatas.push_back(std::move(threadData));

		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Created Thread Data " + std::to_string(i));

		ms_loadThreads.emplace_back(std::jthread(LoadLoop, i));

		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Activated Thread " + std::to_string(i));
	}

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: All Threads Activated!");
}

void LoadManager::EnableStopOnFlush()
{
	if (!SettingsManager::ms_DX12.Async.Enabled || ms_fullShutdownActive)
		return;

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Attempting to enable stop on flush");
	ms_stopOnFlush = true;
	ms_conditionVariable.notify_all();
	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Stop on flush enabled");
}

void LoadManager::StopLoading()
{
	if (!SettingsManager::ms_DX12.Async.Enabled || ms_fullShutdownActive || !ms_loadingActive)
		return;
	
	ms_loadingActive = false;	
	AlkaliGUIManager::LogAsyncMessage("Loading stopping, all threads should begin exiting");	
}

void LoadManager::FullShutdown()
{
	if (ms_fullShutdownActive)
		return;

	ms_fullShutdownActive = true;
	ms_stopOnFlush = false;

	ms_loadingActive = false;
	ms_conditionVariable.notify_all();
	ms_loadThreads.clear();

	ms_threadDatas.clear();
	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Full Shutdown Completed");
}

void LoadManager::LoadLoop(int threadID)
{
	AlkaliGUIManager::LogAsyncMessage("New thread started up: " + std::to_string(threadID));

	while (ms_loadingActive)
	{
		if (!ms_stopOnFlush)
		{
			AlkaliGUIManager::LogAsyncMessage("Thread waiting for signal: " + std::to_string(threadID));
			std::unique_lock<std::mutex> lock(ms_notifyMutex);
			ms_conditionVariable.wait(lock);
			AlkaliGUIManager::LogAsyncMessage("Thread woken up by signal: " + std::to_string(threadID));
		}		

		if (ms_stopOnFlush && AllQueuesFlushed())
		{
			StopLoading();
			return;
		}

		LoadHighestPriority(threadID);
	}
}

void LoadManager::LoadHighestPriority(int threadID)
{	
	std::unique_lock<std::mutex> lockModel(ms_mutexModelQueue);
	if (ms_modelQueue.size() > 0)
	{
		AsyncModelArgs modelArgs = ms_modelQueue.front();
		ms_modelQueue.pop();	
		lockModel.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found model (" + modelArgs.FilePath + ")");

		LoadModel(modelArgs, threadID);

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " loaded model (" + modelArgs.FilePath + ")");
		return;
	}
	lockModel.unlock();

	std::unique_lock<std::mutex> lockTexCubeMap(ms_mutexTexCubemapQueue);
	if (ms_texCubemapQueue.size() > 0)
	{
		AsyncTexCubemapArgs texArgs = ms_texCubemapQueue.front();
		ms_texCubemapQueue.pop();
		lockTexCubeMap.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found cubemap (" + texArgs.FilePath + ")");

		LoadTexCubemap(texArgs, threadID);

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " loaded cubemap (" + texArgs.FilePath + ")");
		return;
	}
	lockTexCubeMap.unlock();

	std::unique_lock<std::mutex> lockTex(ms_mutexTexQueue);
	if (ms_texQueue.size() > 0)
	{
		AsyncTexArgs texArgs = ms_texQueue.front();
		ms_texQueue.pop();
		lockTex.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found tex (" + texArgs.FilePath + ")");

		LoadTex(texArgs, threadID);

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " loaded tex (" + texArgs.FilePath + ")");
		return;
	}
	lockTex.unlock();

	std::unique_lock<std::mutex> lockShader(ms_mutexShaderQueue);
	if (ms_shaderQueue.size() > 0)
	{
		AsyncShaderArgs shaderArgs = ms_shaderQueue.front();
		ms_shaderQueue.pop();
		lockShader.unlock();

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found shader (" + wstringToString(shaderArgs.Args.VS) + ")");

		LoadShader(shaderArgs, threadID);

		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " loaded shader (" + wstringToString(shaderArgs.Args.VS) + ")");
		return;
	}
	lockShader.unlock();

	if (!SettingsManager::ms_DX12.Async.LogIgnoreInfoMessages)
		AlkaliGUIManager::LogAsyncMessage("Thread " + std::to_string(threadID) + " found nothing");
}

void LoadManager::ExecuteCPUWaitingLists()
{
	if (!SettingsManager::ms_DX12.Async.Enabled)
		return;

	if (!SettingsManager::ms_DX12.Async.LogIgnoreInfoMessages)
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: CPU waiting list executing");

	if (!ms_loadingActive && ms_loadThreads.size() > 0 && false)
	{
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Clearing loading threads");
		ms_loadThreads.clear();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Main thread resuming, all threads exited");
	}

	static int framesSinceExecute = 0;
	framesSinceExecute++;
	if (framesSinceExecute < SettingsManager::ms_DX12.Async.DebugExecutionFrameSlice)
		return;

	for (int i = 0; i < ms_numThreads; i++)
	{
		std::unique_lock<std::mutex> lockCPU(ms_mutexThreadDatas[i]);

		bool emptyModelList = ms_threadDatas[i]->CPU_WaitingListModel.size() == 0;
		bool emptyTexList = ms_threadDatas[i]->CPU_WaitingListTexture.size() == 0;

		if (!emptyModelList)
		{
			GPUWaitingList gpuWaitingList;
			gpuWaitingList.ModelList = ms_threadDatas[i]->CPU_WaitingListModel;
			gpuWaitingList.FenceValue = ms_copyQueue->ExecuteCommandList(ms_threadDatas[i]->CmdListCopy);
			gpuWaitingList.IsCOPY = true;
			ms_gpuWaitingLists.push(gpuWaitingList);

			ms_threadDatas[i]->CPU_WaitingListModel.clear();
			ms_threadDatas[i]->CmdListCopy = ms_copyQueue->GetAvailableCommandList();

			string strModelCount = std::to_string(gpuWaitingList.ModelList.size());
			string strFenceVal = std::to_string(gpuWaitingList.FenceValue);
			AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: COPY CPU waiting list cleared, " + strModelCount + " models put on GPU waiting list with fence " + strFenceVal);

			framesSinceExecute = 0;
			if (SettingsManager::ms_DX12.Async.DebugExecutionFrameSlice > 0)
				break;
		}

		if (!emptyTexList)
		{
			GPUWaitingList gpuWaitingList;
			gpuWaitingList.TextureList = ms_threadDatas[i]->CPU_WaitingListTexture;
			gpuWaitingList.FenceValue = ms_computeQueue->ExecuteCommandList(ms_threadDatas[i]->CmdListCompute);
			gpuWaitingList.IsCOPY = false;
			ms_gpuWaitingLists.push(gpuWaitingList);

			ms_threadDatas[i]->CPU_WaitingListTexture.clear();
			ms_threadDatas[i]->CmdListCompute = ms_computeQueue->GetAvailableCommandList();

			string strTexCount = std::to_string(gpuWaitingList.TextureList.size());
			string strFenceVal = std::to_string(gpuWaitingList.FenceValue);
			AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: COMPUTE CPU waiting list cleared, " + strTexCount + " textures put on GPU waiting list with fence " + strFenceVal);

			framesSinceExecute = 0;
			if (SettingsManager::ms_DX12.Async.DebugExecutionFrameSlice > 0)
				break;
		}
	}

	while (ms_gpuWaitingLists.size() > 0)
	{
		auto queue = ms_gpuWaitingLists.front().IsCOPY ? ms_copyQueue : ms_computeQueue;

		// Lowest fence value will always be at the front
		if (!queue->IsFenceComplete(ms_gpuWaitingLists.front().FenceValue))
			return;

		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: GPU waiting list complete with fence " + std::to_string(ms_gpuWaitingLists.front().FenceValue));

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

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Model pushed to queue: " + filePath);

	AsyncModelArgs modelArg;
	modelArg.pModel = pModel;
	modelArg.FilePath = filePath;
	
	if (SettingsManager::ms_DX12.Async.LogMainThreadMutexDelays)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
		auto finish = std::chrono::high_resolution_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Waited " + std::to_string(millis) + "ms to get a mutex");
		ms_modelQueue.push(modelArg);
	}
	else
	{
		std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
		ms_modelQueue.push(modelArg);
	}
	
	ms_conditionVariable.notify_one();

	return true;
}

bool LoadManager::TryPushModel(Model* pModel, Asset asset, size_t meshIndex, size_t primitiveIndex)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("GLTF Model tried to be pushed to queue but async wasn't enabled");
		return false; // Loads syncronously
	}

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: GLTF Model pushed to queue");

	AsyncModelArgs modelArg;
	modelArg.pModel = pModel;
	modelArg.Asset = asset;
	modelArg.MeshIndex = meshIndex;
	modelArg.PrimitiveIndex = primitiveIndex;

	if (SettingsManager::ms_DX12.Async.LogMainThreadMutexDelays)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
		auto finish = std::chrono::high_resolution_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Waited " + std::to_string(millis) + "ms to get a mutex");
		ms_modelQueue.push(modelArg);
	}
	else
	{
		std::unique_lock<std::mutex> lock(ms_mutexModelQueue);
		ms_modelQueue.push(modelArg);
	}

	ms_conditionVariable.notify_one();

	return true;
}

bool LoadManager::TryPushTex(Texture* pTex, string filePath, bool flipUpsideDown, bool isNormalMap, bool disableMips)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Texture tried to be pushed to queue: " + filePath + " but async wasn't enabled");
		return false; // Loads syncronously
	}

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Texture pushed to queue: " + filePath);

	AsyncTexArgs texArgs;
	texArgs.pTexture = pTex;
	texArgs.FilePath = filePath;
	texArgs.FlipUpsideDown = flipUpsideDown;
	texArgs.IsNormalMap = isNormalMap;
	texArgs.DisableMips = disableMips;

	if (SettingsManager::ms_DX12.Async.LogMainThreadMutexDelays)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(ms_mutexTexQueue);
		auto finish = std::chrono::high_resolution_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Waited " + std::to_string(millis) + "ms to get a mutex");
		ms_texQueue.push(texArgs);
	}
	else
	{
		std::unique_lock<std::mutex> lock(ms_mutexTexQueue);
		ms_texQueue.push(texArgs);
	}

	ms_conditionVariable.notify_one();

	return true;
}

bool LoadManager::TryPushCubemap(Texture* pTex, string filePath, Texture* pIrradiance, bool flipUpsideDown)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Cubemap tried to be pushed to queue: " + filePath + " but async wasn't enabled");
		return false; // Loads syncronously
	}

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Cubemap pushed to queue: " + filePath);

	AsyncTexCubemapArgs texArgs;
	texArgs.pCubemap = pTex;
	texArgs.pIrradiance = pIrradiance;
	texArgs.FilePath = filePath;
	texArgs.FlipUpsideDown = flipUpsideDown;

	if (SettingsManager::ms_DX12.Async.LogMainThreadMutexDelays)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(ms_mutexTexCubemapQueue);
		auto finish = std::chrono::high_resolution_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Waited " + std::to_string(millis) + "ms to get a mutex");
		ms_texCubemapQueue.push(texArgs);
	}
	else
	{
		std::unique_lock<std::mutex> lock(ms_mutexTexCubemapQueue);
		ms_texCubemapQueue.push(texArgs);
	}

	ms_conditionVariable.notify_one();

	return true;
}

bool LoadManager::TryPushCubemap(Texture* pTex, vector<string> filePaths, Texture* pIrradiance, bool flipUpsideDown)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Cubemap tried to be pushed to queue: " + filePaths.at(0) + " but async wasn't enabled");
		return false; // Loads syncronously
	}

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Cubemap pushed to queue: " + filePaths.at(0));

	AsyncTexCubemapArgs texArgs;
	texArgs.pCubemap = pTex;
	texArgs.pIrradiance = pIrradiance;
	texArgs.FilePaths = filePaths;
	texArgs.FlipUpsideDown = flipUpsideDown;

	if (SettingsManager::ms_DX12.Async.LogMainThreadMutexDelays)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(ms_mutexTexCubemapQueue);
		auto finish = std::chrono::high_resolution_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Waited " + std::to_string(millis) + "ms to get a mutex");
		ms_texCubemapQueue.push(texArgs);
	}
	else
	{
		std::unique_lock<std::mutex> lock(ms_mutexTexCubemapQueue);
		ms_texCubemapQueue.push(texArgs);
	}

	ms_conditionVariable.notify_one();

	return true;
}

bool LoadManager::TryPushShader(Shader* pShader, const ShaderArgs& shaderArgs)
{
	if (!ms_loadingActive)
	{
		AlkaliGUIManager::LogAsyncMessage("Shader tried to be pushed to queue but async wasn't enabled");
		return false; // Loads syncronously
	}

	AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Shader pushed to queue: " + wstringToString(shaderArgs.VS));

	AsyncShaderArgs args;
	args.pShader = pShader;
	args.Args = shaderArgs;

	if (SettingsManager::ms_DX12.Async.LogMainThreadMutexDelays)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(ms_mutexShaderQueue);
		auto finish = std::chrono::high_resolution_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		AlkaliGUIManager::LogAsyncMessage("MAIN THREAD: Waited " + std::to_string(millis) + "ms to get a mutex");
		ms_shaderQueue.push(args);
	}
	else
	{
		std::unique_lock<std::mutex> lock(ms_mutexShaderQueue);
		ms_shaderQueue.push(args);
	}

	ms_conditionVariable.notify_one();

	return true;
}

void LoadManager::LoadModel(AsyncModelArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis > 0)
		Sleep(static_cast<DWORD>(SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis));

	if (!args.pModel || ms_fullShutdownActive)
		return;

	std::unique_lock<std::mutex> lock(ms_mutexThreadDatas[threadID]);

	if (args.FilePath != "")
	{
		bool success = args.pModel->Init(ms_threadDatas[threadID]->CmdListCopy.Get(), args.FilePath);
		if (!success)
		{
			AlkaliGUIManager::LogErrorMessage("Failed to load model [async] (path=\"" + args.FilePath + "\")");
			return;
		}
	}		
	else if (args.Asset)
	{
		auto& primitive = args.Asset->get().meshes[args.MeshIndex].primitives[args.PrimitiveIndex];
		ModelLoaderGLTF::LoadModel(ms_d3dClass, ms_threadDatas[threadID]->CmdListCopy.Get(), args.Asset, primitive, args.pModel);
	}		
	else
		throw std::exception();
	
	ms_threadDatas[threadID]->CPU_WaitingListModel.push_back(args.pModel);
}

void LoadManager::LoadTex(AsyncTexArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis > 0)
		Sleep(static_cast<DWORD>(SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis));

	if (!args.pTexture || ms_fullShutdownActive)
		return;

	if (args.FilePath == "")
		throw std::exception();

	std::unique_lock<std::mutex> lock(ms_mutexThreadDatas[threadID]);

	auto cmdList = ms_threadDatas[threadID]->CmdListCompute.Get();
	args.pTexture->Init(ms_d3dClass, cmdList, args.FilePath, args.FlipUpsideDown, args.IsNormalMap, args.DisableMips);
	
	ms_threadDatas[threadID]->CPU_WaitingListTexture.push_back(args.pTexture);
}

void LoadManager::LoadTexCubemap(AsyncTexCubemapArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis > 0)
		Sleep(static_cast<DWORD>(SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis));

	if (!args.pCubemap || ms_fullShutdownActive)
		return;

	std::unique_lock<std::mutex> lock(ms_mutexThreadDatas[threadID]);

	auto cmdList = ms_threadDatas[threadID]->CmdListCompute.Get();

	if (args.FilePath == "")
		args.pCubemap->InitCubeMap(ms_d3dClass, cmdList, args.FilePaths, args.FlipUpsideDown);
	else
		args.pCubemap->InitCubeMapHDR(ms_d3dClass, cmdList, args.FilePath, args.FlipUpsideDown);
	
	if (args.pIrradiance && SettingsManager::ms_DX12.Async.DispatchIrradianceMaps)
	{
		args.pIrradiance->InitCubeMapUAV_Empty(ms_d3dClass);
		TextureLoader::CreateIrradianceMap(ms_d3dClass, cmdList, args.pCubemap->GetResource(), args.pIrradiance->GetResource());
	}

	ms_threadDatas[threadID]->CPU_WaitingListTexture.push_back(args.pCubemap);
	if (args.pIrradiance)
		ms_threadDatas[threadID]->CPU_WaitingListTexture.push_back(args.pIrradiance);
}

void LoadManager::LoadShader(AsyncShaderArgs args, int threadID)
{
	if (SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis > 0)
		Sleep(static_cast<DWORD>(SettingsManager::ms_DX12.Async.DebugLoadingDelayMillis));

	if (ms_fullShutdownActive)
		return;

	args.pShader->Init(ms_d3dClass->GetDevice(), args.Args);
}

bool LoadManager::AllQueuesFlushed()
{
	return ms_modelQueue.size() == 0 &&
		ms_texQueue.size() == 0 &&
		ms_texCubemapQueue.size() == 0 &&
		ms_shaderQueue.size() == 0;
}
