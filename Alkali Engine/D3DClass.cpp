#include "pch.h"
#include "D3DClass.h"
#include "ResourceManager.h"

D3DClass::D3DClass()
{
}

D3DClass::~D3DClass()
{    
}

void D3DClass::Init()
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    m_dxgiAdapter = ResourceManager::GetAdapter(USE_BARRIER_WARP);
    if (!m_dxgiAdapter)
        throw new std::exception("Adapter not found");

    m_d3d12Device = ResourceManager::CreateDevice(m_dxgiAdapter.Get());
    if (!m_d3d12Device)
        throw new std::exception("Device failed to be created");

    m_directCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_computeCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
    m_copyCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device.Get(), D3D12_COMMAND_LIST_TYPE_COPY);

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
        assert(false && "Invalid command queue type.");
    }
}

bool D3DClass::IsTearingSupported() const
{
    return m_tearingSupported;
}

void D3DClass::Flush()
{
    m_directCommandQueue->Flush();
    m_computeCommandQueue->Flush();
    m_copyCommandQueue->Flush();
}

UINT D3DClass::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return m_d3d12Device->GetDescriptorHandleIncrementSize(type);
}