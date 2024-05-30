#pragma once

#include "pch.h"

#include "Events.h"

#include <memory> // for std::enable_shared_from_this
#include <string> // for std::wstring
#include <array>

#include "Batch.h"
#include "CommandQueue.h"
#include "Window.h"

using std::array;

class Model;
class Batch;
class CommandQueue;

class Scene : public std::enable_shared_from_this<Scene>
{
public:
    Scene(const std::wstring& name, int width, int height, bool vSync, bool createDSV);
    virtual ~Scene();

    bool Init(shared_ptr<D3DClass> pD3DClass);
    virtual bool LoadContent() = 0;

    virtual void UnloadContent() = 0;
    virtual void Destroy();

    virtual void OnUpdate(UpdateEventArgs& e);
    virtual void OnRender(RenderEventArgs& e);

    virtual void OnKeyPressed(KeyEventArgs& e);
    virtual void OnKeyReleased(KeyEventArgs& e);

    virtual void OnMouseMoved(MouseMotionEventArgs& e);
    virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
    virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
    virtual void OnMouseWheel(MouseWheelEventArgs& e);

    virtual void OnResize(ResizeEventArgs& e);
    virtual void OnWindowDestroy();

protected:
    void SetBackgroundColor(float r, float g, float b, float a);
    
    void ClearBackBuffer(ComPtr<ID3D12GraphicsCommandList2> commandList);
    void ClearDepth(ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

    void Present(ComPtr<ID3D12GraphicsCommandList2> commandList, std::shared_ptr<CommandQueue> commandQueue);

    void SetDSVForSize(int width, int height);

    std::shared_ptr<Window> m_pWindow;

    int m_width;
    int m_height;

    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    shared_ptr<D3DClass> m_d3dClass;

private:
    std::wstring m_name;
    bool m_vSync;    
    bool m_dsvEnabled;
    FLOAT m_backgroundColor[4];

    ComPtr<ID3D12Resource> m_depthBuffer;   

    array<uint64_t, BACK_BUFFER_COUNT> m_FenceValues = {};
};