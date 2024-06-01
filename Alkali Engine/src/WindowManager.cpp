#include "pch.h"
#include "WindowManager.h"
#include "Settings.h"
#include "InputManager.h"

static WindowManager* gs_Instance;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

WindowManager::WindowManager()
{
    if (!gs_Instance)
        gs_Instance = this;
    else
        throw new std::exception("Singleton error in Window Manager");
}

WindowManager::~WindowManager()
{
    assert(m_windowHwndMap.empty() && m_windowNameMap.empty() && "All windows should be destroyed before destroying the application instance.");
}

void WindowManager::Init(HINSTANCE hInstance)
{
    m_hInstance = hInstance;

    WNDCLASSEXW wndClass = { 0 };

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.hInstance = hInstance;
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = WINDOW_CLASS_NAME;
    wndClass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    if (!RegisterClassExW(&wndClass))
    {
        MessageBoxA(NULL, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
    }

    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

WindowManager* WindowManager::GetInstance()
{
    assert(gs_Instance);
    return gs_Instance;
}

shared_ptr<Window> WindowManager::CreateRenderWindow(shared_ptr<D3DClass> pD3D, const wstring& windowName, int clientWidth, int clientHeight, bool vSync)
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

    shared_ptr<Window> pWindow = make_shared<Window>(hWnd, pD3D, windowName, clientWidth, clientHeight, vSync);

    m_windowHwndMap.insert(WindowMap::value_type(hWnd, pWindow));
    m_windowNameMap.insert(WindowNameMap::value_type(windowName, pWindow));

    return pWindow;
}

void WindowManager::DestroyWindow(const wstring& windowName)
{
    shared_ptr<Window> pWindow = GetWindowByName(windowName);
    if (pWindow)
    {
        if (pWindow)
            pWindow->Destroy();
    }
}

void WindowManager::DestroyWindow(shared_ptr<Window> window)
{
    if (window)
        window->Destroy();
}

shared_ptr<Window> WindowManager::GetWindowByName(const wstring& windowName)
{
    shared_ptr<Window> window;
    WindowNameMap::iterator iter = m_windowNameMap.find(windowName);
    if (iter != m_windowNameMap.end())
    {
        window = iter->second;
    }

    return window;
}

shared_ptr<Window> WindowManager::GetWindowByHwnd(const HWND hwnd)
{
    shared_ptr<Window> window;
    WindowMap::iterator iter = m_windowHwndMap.find(hwnd);
    if (iter != m_windowHwndMap.end())
    {
        window = iter->second;
    }

    return window;
}

void WindowManager::RemoveWindow(HWND hWnd)
{
    WindowMap::iterator windowIter = m_windowHwndMap.find(hWnd);
    if (windowIter != m_windowHwndMap.end())
    {
        shared_ptr<Window> pWindow = windowIter->second;
        m_windowNameMap.erase(pWindow->GetWindowName());
        m_windowHwndMap.erase(windowIter);
    }
}

int WindowManager::GetTrackedWindowCount()
{
    return static_cast<int>(m_windowHwndMap.size());
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
    shared_ptr<Window> pWindow = gs_Instance->GetWindowByHwnd(hwnd);

    if (!pWindow)
        return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_PAINT:
    {
        pWindow->OnUpdate();
        pWindow->OnRender();
        InputManager::ProgressFrame();
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        MSG charMsg;

        unsigned int unicodeChar = 0;
        if (PeekMessage(&charMsg, hwnd, 0, 0, PM_NOREMOVE) && charMsg.message == WM_CHAR)
        {
            GetMessage(&charMsg, hwnd, 0, 0);
            unicodeChar = static_cast<unsigned int>(charMsg.wParam);
        }

        KeyCode::Key key = (KeyCode::Key)wParam;

        KeyState state = { key, unicodeChar };
        InputManager::AddKey(state);
    }
    break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        KeyCode::Key key = (KeyCode::Key)wParam;
        unsigned int unicodeChar = 0;
        unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

        unsigned char keyboardState[256];
        GetKeyboardState(keyboardState);

        wchar_t translatedCharacters[4];
        // CHANGE THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // Afraid to do it rn because I don't fully understand it and can't test it yet
        if (int result = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, keyboardState, translatedCharacters, 4, 0, NULL) > 0)
        {
            unicodeChar = translatedCharacters[0];
        }

        KeyState state = { key, unicodeChar };
        InputManager::RemoveKey(state);
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
        gs_Instance->RemoveWindow(hwnd);

        if (gs_Instance->GetTrackedWindowCount() == 0)
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