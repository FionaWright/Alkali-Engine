#pragma once

#include "pch.h"
#include <memory>
#include <string>
#include "Window.h"

using std::wstring;
using std::shared_ptr;
using std::map;

using WindowMap = map< HWND, shared_ptr<Window> >;
using WindowNameMap = map< wstring, shared_ptr<Window> >;

class WindowManager
{
public:
    WindowManager();
    ~WindowManager();

    void Init(HINSTANCE hInstance);

    static WindowManager* GetInstance();

    shared_ptr<Window> CreateRenderWindow(shared_ptr<D3DClass> pD3D, const wstring& windowName, int clientWidth, int clientHeight, bool vSync = true);
    void DestroyWindow(const wstring& windowName);
    void DestroyWindow(shared_ptr<Window> window);

    shared_ptr<Window> GetWindowByName(const wstring& windowName);
    shared_ptr<Window> GetWindowByHwnd(const HWND hwnd);

    void RemoveWindow(HWND hWnd);

    int GetTrackedWindowCount();

private:
    HINSTANCE m_hInstance;

    WindowMap m_windowHwndMap;
    WindowNameMap m_windowNameMap;
};

