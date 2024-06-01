#pragma once

#include "pch.h"

#include <cstdint>  // For uint64_t
#include <queue>    

using std::queue;

class CommandQueue
{
public:
    CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
    virtual ~CommandQueue();

    ComPtr<ID3D12GraphicsCommandList2> GetAvailableCommandList();

    uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

protected:
    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator);

private:
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = queue<CommandAllocatorEntry>;
    using CommandListQueue = queue<ComPtr<ID3D12GraphicsCommandList2> >;

    D3D12_COMMAND_LIST_TYPE                     m_CommandListType;
    ComPtr<ID3D12Device2>                       m_d3d12Device;
    ComPtr<ID3D12CommandQueue>                  m_d3d12CommandQueue;
    ComPtr<ID3D12Fence>                         m_d3d12Fence;
    HANDLE                                      m_FenceEvent;
    uint64_t                                    m_FenceValue;

    CommandAllocatorQueue                       m_CommandAllocatorQueue;
    CommandListQueue                            m_CommandListQueue;
};