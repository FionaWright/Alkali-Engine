#include "pch.h"
#include "ShaderComplexityManager.h"
#include <ResourceTracker.h>
#include <DescriptorManager.h>

unordered_map<Shader*, float> ShaderComplexityManager::ms_complexityTable;
Model* ShaderComplexityManager::ms_model;
uint32_t ShaderComplexityManager::ms_rollingMeanFrameCount;

Material* GetValidMaterial(const unordered_map<string, shared_ptr<Batch>>& batches, Shader* shader, RootSig** ppRS) 
{
	for (auto& batchIT : batches)
	{
		for (auto& opaque : batchIT.second->GetOpaques())
			if (opaque.GetShader() == shader)
			{
				*ppRS = batchIT.second->GetRootSig();
				return opaque.GetMaterial();
			}				

		for (auto& at : batchIT.second->GetATs())
			if (at.GetShader() == shader)
			{
				*ppRS = batchIT.second->GetRootSig();
				return at.GetMaterial();
			}				

		for (auto& trans : batchIT.second->GetTrans())
			if (trans.GetShader() == shader)
			{
				*ppRS = batchIT.second->GetRootSig();
				return trans.GetMaterial();
			}				
	}
	return nullptr;
}

void ShaderComplexityManager::Init(Model* model)
{
	ms_model = model;
	ms_rollingMeanFrameCount = 0;
	ms_complexityTable.clear();
}

void ShaderComplexityManager::CalculateComplexityTable(D3DClass* d3d, int backBufferIndex, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect, const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, const D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle, ID3D12Resource* backBufferResource)
{
	const UINT64 COLOR_PINK = PIX_COLOR(185, 158, 235);

	assert(ms_model);
	if (!ms_model->IsLoaded())
		return;

	unordered_map<Shader*, float> tempComplexityTable;
	CommandQueue* cmdQueue = d3d->GetCommandQueue();

	auto globalHeap = DescriptorManager::GetHeap();
	ID3D12DescriptorHeap* ppHeaps[] = { globalHeap };

	vector<string> permutations;
	if (SettingsManager::ms_Dynamic.Shadow.Enabled)
		permutations.push_back("SHADOW_ENABLED");
	if (SettingsManager::ms_Dynamic.IndirectSpecularEnabled)
		permutations.push_back("INDIRECT_ENABLED");
	if (!SettingsManager::ms_Dynamic.DepthPrePassEnabled)
		permutations.push_back("MAIN_PASS_ALPHA_TEST");

	float rotationX = 90 * static_cast<float>(DEG_TO_RAD);
	MatricesCB matrices;
	//matrices.M = XMMatrixRotationRollPitchYaw(rotationX, 0, 0);
	matrices.M = XMMatrixIdentity();
	matrices.InverseTransposeM = XMMatrixIdentity();
	matrices.V = XMMatrixIdentity();
	matrices.P = XMMatrixIdentity();

	bool transitionedBackBuffer = false;

	auto& shaders = ResourceTracker::GetShaders();
	auto& batches = ResourceTracker::GetBatches();
	for (auto& it : shaders)
	{
		if (!it.second->IsInitialised())
			continue;

		RootSig* pRootSig = nullptr;
		Material* validMat = GetValidMaterial(batches, it.second.get(), &pRootSig);
		if (!validMat || !validMat->IsLoaded())
			continue;
		if (!pRootSig)
			continue;		
		
		auto cmdList = cmdQueue->GetAvailableCommandList();
		PIXBeginEvent(cmdList.Get(), COLOR_PINK, "Shader Complexity Profiling");

		if (!validMat->EnsureCorrectSRVState(cmdList.Get()))
			continue;

		validMat->SetCBV_PerDraw(0, &matrices, sizeof(MatricesCB), backBufferIndex);

		if (!transitionedBackBuffer)
		{
			ResourceManager::TransitionResource(cmdList.Get(), backBufferResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			transitionedBackBuffer = true;
		}

		cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		cmdList->RSSetScissorRects(1, &scissorRect);
		cmdList->RSSetViewports(1, &viewport);
		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);			
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		cmdList->SetGraphicsRootSignature(pRootSig->GetRootSigResource());
		cmdList->SetPipelineState(it.second->GetPSO(permutations).Get());
		validMat->AssignMaterial(cmdList.Get(), pRootSig->GetRootParamInfo(), backBufferIndex);

		ms_model->Render(cmdList.Get());

		d3d->Flush();

		PIXEndEvent(cmdList.Get());
		auto startTimeProfiler = std::chrono::high_resolution_clock::now();

		uint64_t fence = cmdQueue->ExecuteCommandList(cmdList);		
		cmdQueue->WaitForFenceValue(fence);

		std::chrono::duration<double, std::milli> timeTaken = std::chrono::high_resolution_clock::now() - startTimeProfiler;
		tempComplexityTable.emplace(it.second.get(), timeTaken.count());
	}

	if (tempComplexityTable.size() == 0)
		return;

	// Normalise the costs
	float totalCost = 0;
	for (auto& it : tempComplexityTable)
		totalCost += it.second;

	// Average against previous frames to account for lost GPU control
	for (auto& it : tempComplexityTable)
	{
		it.second /= totalCost;
		if (ms_rollingMeanFrameCount == 0)
		{
			ms_complexityTable.emplace(it);
			continue;
		}

		float prevMean = ms_complexityTable.at(it.first);

		float newMean = ((ms_rollingMeanFrameCount * prevMean) + it.second) / (ms_rollingMeanFrameCount + 1);
		ms_complexityTable.at(it.first) = newMean;
	}		
	ms_rollingMeanFrameCount++;
}

float ShaderComplexityManager::GetCostOfShader(Shader* shader)
{
	if (!ms_complexityTable.contains(shader))
		return 0.0f;
	return ms_complexityTable.at(shader);
}

void ShaderComplexityManager::ResetComplexityTable()
{
	ms_complexityTable.clear();
	ms_rollingMeanFrameCount = 0;
}
