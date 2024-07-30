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
#include "DebugLine.h"
#include "CBuffers.h"

using std::unique_ptr;
using std::wstring;
using std::array;

class Model;
class Batch;
class CommandQueue;

class Scene : public std::enable_shared_from_this<Scene>
{
public:
    Scene(const wstring& name, Window* pWindow, bool createDSV);
    virtual ~Scene();

    bool Init(D3DClass* pD3DClass);
    virtual bool LoadContent();

    virtual void UnloadContent();
    virtual void Destroy();

    virtual void OnUpdate(TimeEventArgs& e);
    virtual void OnRender(TimeEventArgs& e);

    virtual void OnResize(ResizeEventArgs& e);
    virtual void OnWindowDestroy();

    static bool IsSphereModeOn(Model** model);
    static void StaticShutdown();

    Window* GetWindow();
    PerFrameCBuffers_PBR& GetPerFrameCBuffers();
    Camera* GetCamera();

    wstring m_Name;
    bool m_ContentLoaded = false;

    XMFLOAT4 m_BackgroundColor;

protected:
    void ClearBackBuffer(ID3D12GraphicsCommandList2* commandList);
    void ClearDepth(ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

    void Present(ID3D12GraphicsCommandList2* commandList, CommandQueue* commandQueue);

    void SetDSVForSize(int width, int height);
    void SetDSVFlags(D3D12_DSV_FLAGS flags);

    DebugLine* AddDebugLine(XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color);
    void RenderDebugLines(ID3D12GraphicsCommandList2* commandListDirect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, const int& backBufferIndex);

    Window* m_pWindow;

    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT m_scissorRect = {};

    XMMATRIX m_viewMatrix;
    XMMATRIX m_projectionMatrix;
    XMMATRIX m_viewProjMatrix;
    XMMATRIX m_viewProjDebugLinesMatrix;

    D3DClass* m_d3dClass = nullptr;
    unique_ptr<Camera> m_camera; // Change this to not be a ptr

    vector<shared_ptr<DebugLine>> m_debugLineList;
    DebugLine* m_debugLineLightDir = nullptr;

    Frustum m_frustum;
    PerFrameCBuffers_PBR m_perFrameCBuffers = {};

private:    
    bool m_dsvEnabled;
    bool m_updated = false;
    int m_shadowMapCounter = INT_MAX;

    array<uint64_t, BACK_BUFFER_COUNT> m_FenceValues = {};    

    shared_ptr<RootSig> m_rootSigLine, m_viewDepthRootSig;
    shared_ptr<Shader> m_shaderLine, m_viewDepthShader;
    shared_ptr<Material> m_matLine;
    RootParamInfo m_rpiLine, m_viewDepthRPI;

    ComPtr<ID3D12Resource> m_depthBufferResource;
    unique_ptr<GameObject> m_viewDepthGO;
    shared_ptr<Material> m_viewDepthMat;

    DepthViewCB m_depthViewCB = {};
    bool m_depthViewChanged = false;

    static shared_ptr<Material> ms_shadowMapMat;
    static shared_ptr<Material> ms_perFramePBRMat;
    static shared_ptr<Model> ms_sphereModel;
};