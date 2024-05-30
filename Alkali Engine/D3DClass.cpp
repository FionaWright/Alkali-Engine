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

    m_dxgiAdapter = ResourceManager::GetAdapter(USE_B_WARP);
    if (!m_dxgiAdapter)
        throw new std::exception("Adapter not found");

    m_d3d12Device = ResourceManager::CreateDevice(m_dxgiAdapter);
    if (!m_d3d12Device)
        throw new std::exception("Device failed to be created");

    m_DirectCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_ComputeCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
    m_CopyCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_COPY);

    m_TearingSupported = ResourceManager::CheckTearingSupport();
}

ComPtr<ID3D12Device2> D3DClass::GetDevice() const
{
    return m_d3d12Device;
}

shared_ptr<CommandQueue> D3DClass::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{
    shared_ptr<CommandQueue> commandQueue;
    switch (type)
    {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        commandQueue = m_DirectCommandQueue;
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        commandQueue = m_ComputeCommandQueue;
        break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        commandQueue = m_CopyCommandQueue;
        break;
    default:
        assert(false && "Invalid command queue type.");
    }

    return commandQueue;
}

bool D3DClass::IsTearingSupported() const
{
    return m_TearingSupported;
}

void D3DClass::Flush()
{
    m_DirectCommandQueue->Flush();
    m_ComputeCommandQueue->Flush();
    m_CopyCommandQueue->Flush();
}

UINT D3DClass::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return m_d3d12Device->GetDescriptorHandleIncrementSize(type);
}