#include "pch.h"
#include "Window.h"
#include "Scene.h"

using std::wstring;

Window::Window(HWND hWnd, D3DClass* pD3DClass, const wstring& windowName, int clientWidth, int clientHeight, bool vSync)
    : m_hWnd(hWnd)
    , m_WindowName(windowName)
    , m_ClientWidth(clientWidth)
    , m_ClientHeight(clientHeight)
    , m_d3dClass(pD3DClass)
{
    m_dxgiSwapChain = CreateSwapChain();

    m_d3d12RTVDescriptorHeap = ResourceManager::CreateDescriptorHeap(BACK_BUFFER_COUNT, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_RTVDescriptorSize = m_d3dClass->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews();
}

Window::~Window()
{
    assert(!m_hWnd && "Use Application::DestroyWindow before destruction.");
}

HWND Window::GetWindowHandle() const
{
    return m_hWnd;
}

const wstring& Window::GetWindowName() const
{
    return m_WindowName;
}

void Window::Show()
{
    ::ShowWindow(m_hWnd, SW_SHOW);
}

void Window::Hide()
{
    ::ShowWindow(m_hWnd, SW_HIDE);
}

void Window::Destroy()
{
    if (m_pScene)
        m_pScene->OnWindowDestroy();

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

int Window::GetClientWidth() const
{
    return m_ClientWidth;
}

int Window::GetClientHeight() const
{
    return m_ClientHeight;
}

void Window::SetFullscreen(bool fullscreen)
{
    if (fullscreen)
    {
        ::GetWindowRect(m_hWnd, &m_WindowRect);

        UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        ::SetWindowLongW(m_hWnd, GWL_STYLE, windowStyle);

        HMONITOR hMonitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        ::GetMonitorInfo(hMonitor, &monitorInfo);

        ::SetWindowPos(m_hWnd, HWND_NOTOPMOST,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(m_hWnd, SW_MAXIMIZE);
        return;
    }
    
    ::SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

    ::SetWindowPos(m_hWnd, HWND_NOTOPMOST,
        m_WindowRect.left,
        m_WindowRect.top,
        m_WindowRect.right - m_WindowRect.left,
        m_WindowRect.bottom - m_WindowRect.top,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);

    ::ShowWindow(m_hWnd, SW_NORMAL);
}

void Window::RegisterCallbacks(Scene* pScene)
{
    m_pScene = pScene;
}

void Window::OnUpdate(TimeEventArgs& e)
{   
    m_FrameCounter++;

    if (m_pScene)
        m_pScene->OnUpdate(e);
}

void Window::OnRender(TimeEventArgs& e)
{   
    if (m_pScene)
        m_pScene->OnRender(e);
}

void Window::OnResize(ResizeEventArgs& e)
{
    if (m_ClientWidth != e.Width || m_ClientHeight != e.Height)
    {
        m_ClientWidth = std::max(1, e.Width);
        m_ClientHeight = std::max(1, e.Height);

        m_d3dClass->Flush();

        for (int i = 0; i < BACK_BUFFER_COUNT; ++i)
        {
            m_d3d12BackBuffers[i].Reset();
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(m_dxgiSwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(m_dxgiSwapChain->ResizeBuffers(BACK_BUFFER_COUNT, m_ClientWidth, m_ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        m_CurrentBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews();
    }

    if (m_pScene)
        m_pScene->OnResize(e);
}

ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
    HRESULT hr;

    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;

#if defined(_DEBUG)
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));
    ThrowIfFailed(hr);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_ClientWidth;
    swapChainDesc.Height = m_ClientHeight;
    swapChainDesc.Format = SettingsManager::ms_DX12.SwapChainFormat;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = m_d3dClass->IsTearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    swapChainDesc.SampleDesc.Count = SettingsManager::ms_DX12.MSAAEnabled ? 4 : 1;

    ID3D12CommandQueue* pCommandQueue = m_d3dClass->GetCommandQueue()->GetD3D12CommandQueue();

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory4->CreateSwapChainForHwnd(pCommandQueue, m_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    ThrowIfFailed(hr);

    hr = dxgiFactory4->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
    ThrowIfFailed(hr);

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    m_CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

    return dxgiSwapChain4;
}

void Window::UpdateRenderTargetViews()
{
    auto device = m_d3dClass->GetDevice();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < BACK_BUFFER_COUNT; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(m_dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = SettingsManager::ms_DX12.RTVFormat;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        device->CreateRenderTargetView(backBuffer.Get(), &desc, rtvHandle);

        m_d3d12BackBuffers[i] = backBuffer;

        rtvHandle.Offset(m_RTVDescriptorSize);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_CurrentBackBufferIndex, m_RTVDescriptorSize);
}

ID3D12Resource* Window::GetCurrentBackBuffer() const
{
    return m_d3d12BackBuffers[m_CurrentBackBufferIndex].Get();
}

ID3D12Resource* Window::GetNextBackBuffer() const
{
    int nextIndex = (m_CurrentBackBufferIndex + 1) % BACK_BUFFER_COUNT;
    return m_d3d12BackBuffers[nextIndex].Get();
}

HWND Window::GetHWND()
{
    return m_hWnd;
}

ID3D12DescriptorHeap* Window::GetRTVDescriptorHeap()
{
    return m_d3d12RTVDescriptorHeap.Get();
}

UINT Window::GetCurrentBackBufferIndex() const
{
    return m_CurrentBackBufferIndex;
}

UINT Window::Present()
{
    UINT syncInterval = SettingsManager::ms_Dynamic.VSyncEnabled ? 1 : 0;
    UINT presentFlags = m_d3dClass->IsTearingSupported() && !SettingsManager::ms_Dynamic.VSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;

    HRESULT hr = m_dxgiSwapChain->Present(syncInterval, presentFlags);
    ThrowIfFailed(hr);

    m_CurrentBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();
    return m_CurrentBackBufferIndex;
}