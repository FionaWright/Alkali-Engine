#pragma once

#include "pch.h"
#include "Settings.h"

#include <memory>
#include <string>

class Window;
class Scene;
class CommandQueue;
class Tutorial2;

using std::wstring;
using std::shared_ptr;
using std::map;

using WindowMap = map< HWND, shared_ptr<Window> >;
using WindowNameMap = map< wstring, shared_ptr<Window> >;

class Application
{
public:
    Application();
    virtual ~Application();

    void Init(HINSTANCE hInst);

    static Application& Get();

    bool IsTearingSupported() const;

    shared_ptr<Window> CreateRenderWindow(const wstring& windowName, int clientWidth, int clientHeight, bool vSync = true);
    void DestroyWindow(const wstring& windowName);
    void DestroyWindow(shared_ptr<Window> window);

    shared_ptr<Window> GetWindowByName(const wstring& windowName);
    shared_ptr<Window> GetWindowByHwnd(const HWND hwnd);

    int Run();
    void Quit(int exitCode = 0);

    ComPtr<ID3D12Device2> GetDevice() const;
    wstring GetEXEDirectoryPath();
    shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    void Flush();
    
    UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;    

    void RemoveWindow(HWND hWnd);

    int GetTrackedWindowCount();

private:        
    HINSTANCE m_hInstance;

    ComPtr<IDXGIAdapter4> m_dxgiAdapter;
    ComPtr<ID3D12Device2> m_d3d12Device;

    shared_ptr<CommandQueue> m_DirectCommandQueue;
    shared_ptr<CommandQueue> m_ComputeCommandQueue;
    shared_ptr<CommandQueue> m_CopyCommandQueue;

    shared_ptr<Tutorial2> m_tutScene;

    bool m_TearingSupported;

    wstring m_exeDirectoryPath;

    WindowMap m_windowHwndMap;
    WindowNameMap m_windowNameMap;

};