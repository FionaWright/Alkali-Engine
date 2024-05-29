/**
* The application class is used to create windows for our application.
*/
#pragma once

#include "pch.h"

#include <memory>
#include <string>

class Window;
class Scene;
class CommandQueue;

using std::wstring;

class Application
{
public:
    static void Create(HINSTANCE hInst);
    static void Destroy();

    static Application& Get();

    bool IsTearingSupported() const;

    /**
    * Create a new DirectX11 render window instance.
    * @param windowName The name of the window. This name will appear in the title bar of the window. This name should be unique.
    * @param clientWidth The width (in pixels) of the window's client area.
    * @param clientHeight The height (in pixels) of the window's client area.
    * @param vSync Should the rendering be synchronized with the vertical refresh rate of the screen.
    * @param windowed If true, the window will be created in windowed mode. If false, the window will be created full-screen.
    * @returns The created window instance. If an error occurred while creating the window an invalid
    * window instance is returned. If a window with the given name already exists, that window will be
    * returned.
    */
    std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync = true);
    void DestroyWindow(const std::wstring& windowName);
    void DestroyWindow(std::shared_ptr<Window> window);

    std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName);

    int Run(std::shared_ptr<Scene> pGame);
    void Quit(int exitCode = 0);

    ComPtr<ID3D12Device2> GetDevice() const;
    wstring GetEXEDirectoryPath();
    std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    void Flush();

    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
    UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

protected:
    Application(HINSTANCE hInst);
    virtual ~Application();

    ComPtr<IDXGIAdapter4> GetAdapter(bool bUseWarp);
    ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);
    bool CheckTearingSupport();

private:
    Application(const Application& copy) = delete;
    Application& operator=(const Application& other) = delete;

    HINSTANCE m_hInstance;

    ComPtr<IDXGIAdapter4> m_dxgiAdapter;
    ComPtr<ID3D12Device2> m_d3d12Device;

    std::shared_ptr<CommandQueue> m_DirectCommandQueue;
    std::shared_ptr<CommandQueue> m_ComputeCommandQueue;
    std::shared_ptr<CommandQueue> m_CopyCommandQueue;

    bool m_TearingSupported;

    wstring m_exeDirectoryPath;

};