#pragma once

#include "pch.h"

#include "Events.h"
#include "HighResolutionClock.h"

#include "CommandQueue.h"
#include "D3DClass.h"
#include "Constants.h"
#include "SettingsManager.h"

using std::wstring;
using std::shared_ptr;
using std::weak_ptr;

class Scene;

class Window
{
public:
    Window(HWND hWnd, D3DClass* pD3DClass, const wstring& windowName, int clientWidth, int clientHeight, bool vSync);
    virtual ~Window();

    HWND GetWindowHandle() const;
    void Destroy();

    const wstring& GetWindowName() const;

    int GetClientWidth() const;
    int GetClientHeight() const;

    void SetFullscreen(bool fullscreen);

    void Show();
    void Hide();

    void OnUpdate(TimeEventArgs& args);
    void OnRender(TimeEventArgs& args);

    UINT GetCurrentBackBufferIndex() const;

    UINT Present();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;
    ID3D12Resource* GetCurrentBackBuffer() const;
    ID3D12Resource* GetNextBackBuffer() const;
    HWND GetHWND();
    ID3D12DescriptorHeap* GetRTVDescriptorHeap();

    void RegisterCallbacks(Scene* pScene);

protected:
    friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    virtual void OnResize(ResizeEventArgs& e);

    ComPtr<IDXGISwapChain4> CreateSwapChain();

    void UpdateRenderTargetViews();

private:
    // Windows should not be copied.
    Window(const Window& copy) = delete;
    Window& operator=(const Window& other) = delete;

    HWND m_hWnd;

    wstring m_WindowName;

    int m_ClientWidth;
    int m_ClientHeight;

    uint64_t m_FrameCounter = 0;

    Scene* m_pScene = nullptr;

    ComPtr<IDXGISwapChain4> m_dxgiSwapChain;
    ComPtr<ID3D12DescriptorHeap> m_d3d12RTVDescriptorHeap;
    ComPtr<ID3D12Resource> m_d3d12BackBuffers[BACK_BUFFER_COUNT];

    UINT m_RTVDescriptorSize;
    UINT m_CurrentBackBufferIndex;

    RECT m_WindowRect;

    D3DClass* m_d3dClass = nullptr;
};