#pragma once

#include "pch.h"

#include <cstdint>  // For uint64_t
#include <queue>    

using std::queue;

class CommandQueue
{
public:
    CommandQueue(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type, bool highPriority);
    virtual ~CommandQueue();

    ComPtr<ID3D12GraphicsCommandList2> GetAvailableCommandList();

    uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> cmdList);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    ID3D12CommandQueue* GetD3D12CommandQueue() const;

protected:
    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator);

private:
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    D3D12_COMMAND_LIST_TYPE                     m_CommandListType;
    ID3D12Device2*                              m_device;
    ComPtr<ID3D12CommandQueue>                  m_d3d12CommandQueue;
    ComPtr<ID3D12Fence>                         m_d3d12Fence;
    HANDLE                                      m_FenceEvent;
    uint64_t                                    m_FenceValue;

    queue<CommandAllocatorEntry>                m_CommandAllocatorQueue;
    queue<ComPtr<ID3D12GraphicsCommandList2>>   m_CommandListQueue;
};