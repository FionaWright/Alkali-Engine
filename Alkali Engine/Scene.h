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
    virtual bool LoadContent() = 0;

    virtual void UnloadContent() = 0;
    virtual void Destroy();

    virtual void OnUpdate(TimeEventArgs& e);
    virtual void OnRender(TimeEventArgs& e);

    virtual void OnResize(ResizeEventArgs& e);
    virtual void OnWindowDestroy();

    void InstantiateCubes(int count);

    static bool IsSphereModeOn(Model** model);
    static bool IsForceReloadBinTex();
    static bool IsMipMapDebugMode();

    Window* GetWindow();

    wstring m_Name;
    bool m_ContentLoaded = false;

protected:
    void SetBackgroundColor(float r, float g, float b, float a);
    
    void ClearBackBuffer(ID3D12GraphicsCommandList2* commandList);
    void ClearDepth(ID3D12GraphicsCommandList2* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

    void Present(ID3D12GraphicsCommandList2* commandList, CommandQueue* commandQueue);

    void SetDSVForSize(int width, int height);
    void SetDSVFlags(D3D12_DSV_FLAGS flags);

    DebugLine* AddDebugLine(XMFLOAT3 start, XMFLOAT3 end, XMFLOAT3 color);
    void RenderDebugLines(ID3D12GraphicsCommandList2* commandListDirect, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
    void RenderImGui();

    Window* m_pWindow;

    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    XMMATRIX m_viewMatrix;
    XMMATRIX m_projectionMatrix;
    XMMATRIX m_viewProjMatrix;

    D3DClass* m_d3dClass;
    unique_ptr<Camera> m_camera; // Change this to not be a ptr

    vector<shared_ptr<DebugLine>> m_debugLineList;
    DebugLine* m_debugLineLightDir;

    float m_FoV;
    Frustum m_frustum;
    PerFrameCBuffers m_perFrameCBuffers;

private:    
    bool m_dsvEnabled;
    bool m_showImGUIDemo = false;
    float m_backgroundColor[4];

    ComPtr<ID3D12Resource> m_depthBuffer;   

    array<uint64_t, BACK_BUFFER_COUNT> m_FenceValues = {};    
    bool m_freezeFrustum = false;

    ComPtr<ID3D12RootSignature> m_rootSigLine, m_rootSigDepth;
    shared_ptr<Shader> m_shaderLine, m_shaderDepth;

    unique_ptr<GameObject> m_goDepthTex;
    shared_ptr<Material> m_matDepthTex;

    static shared_ptr<Model> ms_sphereModel;
    static bool ms_sphereMode;
    static bool ms_visualizeDSV;
    static bool ms_sortBatchGos;
    static bool ms_forceReloadBinTex;
    static bool ms_mipMapDebugMode;
};