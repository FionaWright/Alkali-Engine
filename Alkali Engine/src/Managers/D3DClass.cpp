#include "pch.h"
#include "D3DClass.h"
#include "ResourceManager.h"
#include <AlkaliGUIManager.h>

D3DClass::D3DClass()
{
}

D3DClass::~D3DClass()
{    
    Shutdown();
}

void D3DClass::Init()
{
    bool debugMode = false;
#ifdef _DEBUG
    debugMode = true;
#endif

    if (debugMode || SettingsManager::ms_DX12.EnableValidationLayerOnReleaseMode)
    {
        ComPtr<ID3D12Debug> debugInterface;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
        debugInterface->EnableDebugLayer();
    }    

    if (!XMVerifyCPUSupport())
    {
        MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    m_dxgiAdapter = ResourceManager::GetAdapter(SettingsManager::ms_Window.UseBarrierWarps);
    if (!m_dxgiAdapter)
        throw new std::exception("Adapter not found");

    m_d3d12Device = ResourceManager::CreateDevice(m_dxgiAdapter.Get());
    if (!m_d3d12Device)
        throw new std::exception("Device failed to be created");

    m_directCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, false, 0);
    m_computeCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE, false, 5000);
    m_copyCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device.Get(), D3D12_COMMAND_LIST_TYPE_COPY, false, 20000);

    m_tearingSupported = ResourceManager::CheckTearingSupport();
}

ID3D12Device2* D3DClass::GetDevice() const
{
    return m_d3d12Device.Get();
}

CommandQueue* D3DClass::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{
    switch (type)
    {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        return m_directCommandQueue.get();
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        return m_computeCommandQueue.get();
    case D3D12_COMMAND_LIST_TYPE_COPY:
        return m_copyCommandQueue.get();
    default:
        throw std::exception("Invalid command queue args");
    }    
}

bool D3DClass::IsTearingSupported() const
{
    return m_tearingSupported;
}

void D3DClass::Flush()
{
    AlkaliGUIManager::LogUntaggedMessage("Flushing GPU");

    m_directCommandQueue->Flush();
    m_computeCommandQueue->Flush();
    m_copyCommandQueue->Flush();

    AlkaliGUIManager::LogUntaggedMessage("Flushing Finished");
}

UINT D3DClass::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return m_d3d12Device->GetDescriptorHandleIncrementSize(type);
}

void D3DClass::Shutdown()
{
}
