#pragma once

#include "pch.h"
#include "CommandQueue.h"

using std::shared_ptr;

class D3DClass
{
public:
    D3DClass();
    ~D3DClass();

    void Init();

    ComPtr<ID3D12Device2> GetDevice() const;
    shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    bool IsTearingSupported() const;

    void Flush();

    UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;    

private:   
    ComPtr<IDXGIAdapter4> m_dxgiAdapter;
    ComPtr<ID3D12Device2> m_d3d12Device;

    shared_ptr<CommandQueue> m_DirectCommandQueue;
    shared_ptr<CommandQueue> m_ComputeCommandQueue;
    shared_ptr<CommandQueue> m_CopyCommandQueue;

    bool m_TearingSupported;
};

