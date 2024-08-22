#include "pch.h"
#include "CommandQueue.h"

CommandQueue::CommandQueue(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type, bool highPriority, int initalFenceValue)
    : m_FenceValue(0)
    , m_CommandListType(type)
    , m_device(device)
{
    HRESULT hr;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = highPriority ? D3D12_COMMAND_QUEUE_PRIORITY_HIGH : D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    hr = m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_d3d12CommandQueue));
    ThrowIfFailed(hr);

    m_FenceValue = initalFenceValue;
    hr = m_device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence));
    ThrowIfFailed(hr);

    m_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(m_FenceEvent && "Failed to create fence event handle.");
}

CommandQueue::~CommandQueue()
{
}

ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    HRESULT hr = m_device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&commandAllocator));
    ThrowIfFailed(hr);

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator)
{
    ComPtr<ID3D12GraphicsCommandList2> cmdList;
    HRESULT hr = m_device->CreateCommandList(0, m_CommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
    ThrowIfFailed(hr);

    return cmdList;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetAvailableCommandList()
{
    HRESULT hr;

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList2> cmdList;

    if (!m_CommandAllocatorQueue.empty() && IsFenceComplete(m_CommandAllocatorQueue.front().fenceValue))
    {
        commandAllocator = m_CommandAllocatorQueue.front().commandAllocator;
        m_CommandAllocatorQueue.pop();

        hr = commandAllocator->Reset();
        ThrowIfFailed(hr);
    }
    else
    {
        commandAllocator = CreateCommandAllocator();
    }

    if (!m_CommandListQueue.empty())
    {
        cmdList = m_CommandListQueue.front();
        m_CommandListQueue.pop();

        hr = cmdList->Reset(commandAllocator.Get(), nullptr);
        ThrowIfFailed(hr);
    }
    else
    {
        cmdList = CreateCommandList(commandAllocator);
    }

    // Associate the command allocator with the command list so that it can be
    // retrieved when the command list is executed.
    hr = cmdList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get());
    ThrowIfFailed(hr);

    return cmdList;
}

uint64_t CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> cmdList)
{
    HRESULT hr;

    cmdList->Close();

    ID3D12CommandAllocator* commandAllocator;
    UINT dataSize = sizeof(commandAllocator);

    hr = cmdList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator);
    ThrowIfFailed(hr);

    ID3D12CommandList* const ppCommandLists[] = { cmdList.Get() };

    m_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
    uint64_t fenceValue = Signal();

    m_CommandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });
    m_CommandListQueue.push(cmdList);

    // The ownership of the command allocator has been transferred to the ComPtr
    // in the command allocator queue. It is safe to release the reference 
    // in this temporary COM pointer here.
    commandAllocator->Release();

    return fenceValue;
}

uint64_t CommandQueue::Signal()
{
    m_FenceValue++;

    HRESULT hr = m_d3d12CommandQueue->Signal(m_d3d12Fence.Get(), m_FenceValue);
    ThrowIfFailed(hr);

    return m_FenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
    return m_d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
    if (IsFenceComplete(fenceValue))
        return;       

    HRESULT hr = m_d3d12Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);
    ThrowIfFailed(hr);

    std::chrono::milliseconds duration = std::chrono::milliseconds::max();
    ::WaitForSingleObject(m_FenceEvent, static_cast<DWORD>(duration.count()));
}

void CommandQueue::Flush()
{
    uint64_t fenceValueForSignal = Signal();
    WaitForFenceValue(fenceValueForSignal);
}

ID3D12CommandQueue* CommandQueue::GetD3D12CommandQueue() const
{
    return m_d3d12CommandQueue.Get();
}
