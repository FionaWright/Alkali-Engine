#pragma once

#include "pch.h"

#include "Events.h"
#include "HighResolutionClock.h"

#include "CommandQueue.h"
#include "D3DClass.h"
#include "Settings.h"

using std::wstring;

class Scene;

class Window
{
public:
    Window(HWND hWnd, shared_ptr<D3DClass> pD3DClass, const wstring& windowName, int clientWidth, int clientHeight, bool vSync);
    virtual ~Window();

    HWND GetWindowHandle() const;
    void Destroy();

    const wstring& GetWindowName() const;

    int GetClientWidth() const;
    int GetClientHeight() const;

    bool IsVSync() const;
    void SetVSync(bool vSync);
    void ToggleVSync();

    bool IsFullScreen() const;
    void SetFullscreen(bool fullscreen);
    void ToggleFullscreen();

    void Show();
    void Hide();

    UINT GetCurrentBackBufferIndex() const;

    UINT Present();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

    ComPtr<ID3D12Resource> GetCurrentBackBuffer() const;

    void RegisterCallbacks(std::shared_ptr<Scene> pScene);

protected:
    // The Window procedure needs to call protected methods of this class.
    friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    virtual void OnUpdate();
    virtual void OnRender();

    virtual void OnKeyPressed(KeyEventArgs& e);
    virtual void OnKeyReleased(KeyEventArgs& e);

    virtual void OnMouseMoved(MouseMotionEventArgs& e);
    virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
    virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
    virtual void OnMouseWheel(MouseWheelEventArgs& e);

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
    bool m_VSync;
    bool m_Fullscreen;

    HighResolutionClock m_UpdateClock;
    HighResolutionClock m_RenderClock;
    uint64_t m_FrameCounter = 0;

    std::weak_ptr<Scene> m_pScene;

    ComPtr<IDXGISwapChain4> m_dxgiSwapChain;
    ComPtr<ID3D12DescriptorHeap> m_d3d12RTVDescriptorHeap;
    ComPtr<ID3D12Resource> m_d3d12BackBuffers[BACK_BUFFER_COUNT];

    UINT m_RTVDescriptorSize;
    UINT m_CurrentBackBufferIndex;

    RECT m_WindowRect;

    shared_ptr<D3DClass> m_d3dClass;

};