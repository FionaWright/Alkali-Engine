#pragma once

#include "pch.h"

#include "Events.h"

#include <memory> // for std::enable_shared_from_this
#include <string> // for std::wstring
#include <array>

#include "Batch.h"
#include "CommandQueue.h"
#include "Window.h"
#include "InputManager.h"
#include "Camera.h"
#include "Frustum.h"

using std::unique_ptr;
using std::wstring;
using std::array;

class Model;
class Batch;
class CommandQueue;

class Scene : public std::enable_shared_from_this<Scene>
{
public:
    Scene(const wstring& name, shared_ptr<Window> pWindow, bool createDSV);
    virtual ~Scene();

    bool Init(shared_ptr<D3DClass> pD3DClass);
    virtual bool LoadContent() = 0;

    virtual void UnloadContent() = 0;
    virtual void Destroy();

    virtual void OnUpdate(TimeEventArgs& e);
    virtual void OnRender(TimeEventArgs& e);

    virtual void OnResize(ResizeEventArgs& e);
    virtual void OnWindowDestroy();

    shared_ptr<Window> GetWindow();

    wstring m_Name;
    bool m_ContentLoaded = false;

protected:
    void SetBackgroundColor(float r, float g, float b, float a);
    
    void ClearBackBuffer(ID3D12GraphicsCommandList2* commandList);
    void ClearDepth(ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

    void Present(ID3D12GraphicsCommandList2* commandList, CommandQueue* commandQueue);

    void SetDSVForSize(int width, int height);

    std::shared_ptr<Window> m_pWindow;

    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    XMMATRIX m_viewMatrix;
    XMMATRIX m_projectionMatrix;
    XMMATRIX m_viewProjMatrix;

    shared_ptr<D3DClass> m_d3dClass;
    unique_ptr<Camera> m_camera; // Change this to not be a ptr

    vector<GameObject*> m_gameObjectList;

    float m_FoV;
    Frustum m_frustum;

private:    
    bool m_dsvEnabled;
    bool m_showImGUIDemo = false;
    float m_backgroundColor[4];

    ComPtr<ID3D12Resource> m_depthBuffer;   

    array<uint64_t, BACK_BUFFER_COUNT> m_FenceValues = {};    
    bool m_updatingFrustum = true;
};