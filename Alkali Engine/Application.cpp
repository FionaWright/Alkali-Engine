#include "pch.h"
#include "Application.h"
#include <Shlwapi.h>

#include "Window.h"
#include "CommandQueue.h"
#include "Scene.h"
#include "Tutorial2.h"

static Application* gs_Singleton = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam); // Is this needed?

Application::Application()
{
}

Application::~Application()
{
    assert(m_windowHwndMap.empty() && m_windowNameMap.empty() && "All windows should be destroyed before destroying the application instance.");

    Flush();
}

void Application::Init(HINSTANCE hInst)
{
    if (gs_Singleton)
        throw new std::exception("Tried to initialise multiple applications at once");

    gs_Singleton = this;

    m_hInstance = hInst;
    m_TearingSupported = false;

    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];
    if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
    {
        PathRemoveFileSpecW(path);
        m_exeDirectoryPath.assign(path);
    }

    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    WNDCLASSEXW wndClass = { 0 };

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.hInstance = m_hInstance;
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = WINDOW_CLASS_NAME;
    wndClass.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));

    if (!RegisterClassExW(&wndClass))
    {
        MessageBoxA(NULL, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
    }

    m_dxgiAdapter = GetAdapter(false);
    if (m_dxgiAdapter)
    {
        m_d3d12Device = CreateDevice(m_dxgiAdapter);
    }

    if (m_d3d12Device)
    {
        m_DirectCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        m_ComputeCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
        m_CopyCommandQueue = std::make_shared<CommandQueue>(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_COPY);

        m_TearingSupported = CheckTearingSupport();
    }
}

Application& Application::Get()
{
    assert(gs_Singleton);
    return *gs_Singleton;
}

ComPtr<IDXGIAdapter4> Application::GetAdapter(bool bUseWarp)
{
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;

#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (bUseWarp)
    {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));

        return dxgiAdapter4;
    }
    
    SIZE_T maxDedicatedVideoMemory = 0;
    for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
        dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

        // Check to see if the adapter can create a D3D12 device without actually 
        // creating it. The adapter with the largest dedicated video memory
        // is favored.
        bool flagsMatch = (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
        bool deviceCreatedSuccessfully = SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
        bool enoughMemory = dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory;
        if (flagsMatch && deviceCreatedSuccessfully && enoughMemory)
        {
            maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
            ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
        }
    }

    return dxgiAdapter4;
}

ComPtr<ID3D12Device2> Application::CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
    HRESULT hr;

    ComPtr<ID3D12Device2> d3d12Device2;
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2));
    ThrowIfFailed(hr);    

#if defined(_DEBUG)
    d3d12Device2->SetName(L"DX12 Device2 Object");

    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);        

        D3D12_MESSAGE_ID DenyIds[] =
        {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        NewFilter.DenyList.NumSeverities = _countof(SEVERITIES);
        NewFilter.DenyList.pSeverityList = const_cast<D3D12_MESSAGE_SEVERITY*>(SEVERITIES);

        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return d3d12Device2;
}

bool Application::CheckTearingSupport()
{
    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update.

    ComPtr<IDXGIFactory4> factory4;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory4));
    if (!SUCCEEDED(hr))
        return false;
  
    ComPtr<IDXGIFactory5> factory5;
    hr = factory4.As(&factory5);

    if (!SUCCEEDED(hr))
        return false;

    BOOL allowTearing = FALSE;
    factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    return allowTearing == TRUE;
}

bool Application::IsTearingSupported() const
{
    return m_TearingSupported;
}

shared_ptr<Window> Application::CreateRenderWindow(const wstring& windowName, int clientWidth, int clientHeight, bool vSync)
{
    WindowNameMap::iterator windowIter = m_windowNameMap.find(windowName);
    if (windowIter != m_windowNameMap.end())
    {
        return windowIter->second;
    }

    RECT windowRect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    HWND parent = nullptr;
    HMENU menu = nullptr;
    LPVOID param = nullptr;
    HWND hWnd = CreateWindowW(WINDOW_CLASS_NAME, windowName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, parent, menu, m_hInstance, param);

    if (!hWnd)
    {
        MessageBoxA(NULL, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
        return nullptr;
    }

    shared_ptr<Window> pWindow = make_shared<Window>(hWnd, windowName, clientWidth, clientHeight, vSync);

    m_windowHwndMap.insert(WindowMap::value_type(hWnd, pWindow));
    m_windowNameMap.insert(WindowNameMap::value_type(windowName, pWindow));

    return pWindow;
}

void Application::DestroyWindow(shared_ptr<Window> window)
{
    if (window) 
        window->Destroy();
}

void Application::DestroyWindow(const wstring& windowName)
{
    shared_ptr<Window> pWindow = GetWindowByName(windowName);
    if (pWindow)
    {
        DestroyWindow(pWindow);
    }
}

shared_ptr<Window> Application::GetWindowByName(const wstring& windowName)
{
    shared_ptr<Window> window;
    WindowNameMap::iterator iter = m_windowNameMap.find(windowName);
    if (iter != m_windowNameMap.end())
    {
        window = iter->second;
    }

    return window;
}

shared_ptr<Window> Application::GetWindowByHwnd(HWND hwnd)
{
    shared_ptr<Window> window;
    WindowMap::iterator iter = m_windowHwndMap.find(hwnd);
    if (iter != m_windowHwndMap.end())
    {
        window = iter->second;
    }

    return window;
}

int Application::Run()
{
    m_tutScene = std::make_shared<Tutorial2>(L"Cube Scene", SCREEN_WIDTH, SCREEN_HEIGHT);

    if (!m_tutScene->Initialize())
        return 1;

    if (!m_tutScene->LoadContent())
        return 2;

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    Flush();

    m_tutScene->UnloadContent();
    m_tutScene->Destroy();

    return static_cast<int>(msg.wParam);
}

void Application::Quit(int exitCode)
{
    PostQuitMessage(exitCode);
}

ComPtr<ID3D12Device2> Application::GetDevice() const
{
    return m_d3d12Device;
}

wstring Application::GetEXEDirectoryPath()
{
    return m_exeDirectoryPath;
}

shared_ptr<CommandQueue> Application::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{
    shared_ptr<CommandQueue> commandQueue;
    switch (type)
    {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        commandQueue = m_DirectCommandQueue;
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        commandQueue = m_ComputeCommandQueue;
        break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        commandQueue = m_CopyCommandQueue;
        break;
    default:
        assert(false && "Invalid command queue type.");
    }

    return commandQueue;
}

void Application::Flush()
{
    m_DirectCommandQueue->Flush();
    m_ComputeCommandQueue->Flush();
    m_CopyCommandQueue->Flush();
}

UINT Application::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return m_d3d12Device->GetDescriptorHandleIncrementSize(type);
}

void Application::RemoveWindow(HWND hWnd)
{
    WindowMap::iterator windowIter = m_windowHwndMap.find(hWnd);
    if (windowIter != m_windowHwndMap.end())
    {
        shared_ptr<Window> pWindow = windowIter->second;
        m_windowNameMap.erase(pWindow->GetWindowName());
        m_windowHwndMap.erase(windowIter);
    }
}

int Application::GetTrackedWindowCount()
{
    return m_windowHwndMap.size();
}

MouseButtonEventArgs::MouseButton DecodeMouseButton(UINT messageID)
{
    switch (messageID)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            return MouseButtonEventArgs::Left;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
            return MouseButtonEventArgs::Right;

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            return MouseButtonEventArgs::Middle;
    }

    return MouseButtonEventArgs::None;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    shared_ptr<Window> pWindow = Application::Get().GetWindowByHwnd(hwnd);

    if (!pWindow)
        return DefWindowProcW(hwnd, message, wParam, lParam);
    
    switch (message)
    {
        case WM_PAINT:
        {
            pWindow->OnUpdate();
            pWindow->OnRender();
        }
        break;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            MSG charMsg;

            // Get the Unicode character (UTF-16)
            unsigned int unicodeChar = 0;

            // For printable characters, the next message will be WM_CHAR.
            // This message contains the character code we need to send the KeyPressed event.
            // Inspired by the SDL 1.2 implementation.
            if (PeekMessage(&charMsg, hwnd, 0, 0, PM_NOREMOVE) && charMsg.message == WM_CHAR)
            {
                GetMessage(&charMsg, hwnd, 0, 0);
                unicodeChar = static_cast<unsigned int>(charMsg.wParam);
            }

            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            KeyCode::Key key = (KeyCode::Key)wParam;
            unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

            KeyEventArgs keyEventArgs(key, unicodeChar, KeyEventArgs::Pressed, shift, control, alt);
            pWindow->OnKeyPressed(keyEventArgs);
        }
        break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            KeyCode::Key key = (KeyCode::Key)wParam;
            unsigned int unicodeChar = 0;
            unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

            // Determine which key was released by converting the key code and the scan code
            // to a printable character (if possible).
            // Inspired by the SDL 1.2 implementation.
            unsigned char keyboardState[256];
            GetKeyboardState(keyboardState);

            wchar_t translatedCharacters[4];
            // CHANGE THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // Afraid to do it rn because I don't fully understand it and can't test it yet
            if (int result = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, keyboardState, translatedCharacters, 4, 0, NULL) > 0)
            {
                unicodeChar = translatedCharacters[0];
            }

            KeyEventArgs keyEventArgs(key, unicodeChar, KeyEventArgs::Released, shift, control, alt);
            pWindow->OnKeyReleased(keyEventArgs);
        }
        break;

        // The default window procedure will play a system notification sound 
        // when pressing the Alt+Enter keyboard combination if this message is not handled
        case WM_SYSCHAR:
            break;

        case WM_MOUSEMOVE:
        {
            bool lButton = (wParam & MK_LBUTTON) != 0;
            bool rButton = (wParam & MK_RBUTTON) != 0;
            bool mButton = (wParam & MK_MBUTTON) != 0;
            bool shift = (wParam & MK_SHIFT) != 0;
            bool control = (wParam & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            MouseMotionEventArgs mouseMotionEventArgs(lButton, mButton, rButton, control, shift, x, y);
            pWindow->OnMouseMoved(mouseMotionEventArgs);
        }
        break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
            bool lButton = (wParam & MK_LBUTTON) != 0;
            bool rButton = (wParam & MK_RBUTTON) != 0;
            bool mButton = (wParam & MK_MBUTTON) != 0;
            bool shift = (wParam & MK_SHIFT) != 0;
            bool control = (wParam & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Pressed, lButton, mButton, rButton, control, shift, x, y);
            pWindow->OnMouseButtonPressed(mouseButtonEventArgs);
        }
        break;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        {
            bool lButton = (wParam & MK_LBUTTON) != 0;
            bool rButton = (wParam & MK_RBUTTON) != 0;
            bool mButton = (wParam & MK_MBUTTON) != 0;
            bool shift = (wParam & MK_SHIFT) != 0;
            bool control = (wParam & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Released, lButton, mButton, rButton, control, shift, x, y);
            pWindow->OnMouseButtonReleased(mouseButtonEventArgs);
        }
        break;

        case WM_MOUSEWHEEL:
        {
            float zDelta = ((int)(short)HIWORD(wParam)) / (float)WHEEL_DELTA;
            short keyStates = (short)LOWORD(wParam);

            bool lButton = (keyStates & MK_LBUTTON) != 0;
            bool rButton = (keyStates & MK_RBUTTON) != 0;
            bool mButton = (keyStates & MK_MBUTTON) != 0;
            bool shift = (keyStates & MK_SHIFT) != 0;
            bool control = (keyStates & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            POINT clientToScreenPoint;
            clientToScreenPoint.x = x;
            clientToScreenPoint.y = y;
            ScreenToClient(hwnd, &clientToScreenPoint);

            MouseWheelEventArgs mouseWheelEventArgs(zDelta, lButton, mButton, rButton, control, shift, (int)clientToScreenPoint.x, (int)clientToScreenPoint.y);
            pWindow->OnMouseWheel(mouseWheelEventArgs);
        }
        break;

        case WM_SIZE:
        {
            int width = ((int)(short)LOWORD(lParam));
            int height = ((int)(short)HIWORD(lParam));

            ResizeEventArgs resizeEventArgs(width, height);
            pWindow->OnResize(resizeEventArgs);
        }
        break;

        case WM_DESTROY:
        {
            Application::Get().RemoveWindow(hwnd);

            if (Application::Get().GetTrackedWindowCount() == 0)
            {
                PostQuitMessage(0);
            }
        }
        break;

        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return 0;
}