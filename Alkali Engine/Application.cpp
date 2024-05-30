#include "pch.h"
#include "Application.h"
#include <Shlwapi.h>

#include "Window.h"
#include "CommandQueue.h"
#include "Scene.h"
#include "Tutorial2.h"
#include "ModelLoader.h"

Application::Application()
{
    //ModelLoader::PreprocessObjFile(L"C:\\Users\\finnw\\OneDrive\\Documents\\3D objects\\Madeline.obj");
}

Application::~Application()
{
    m_d3dClass->Flush();
    ResourceManager::Shutdown();
}

void Application::Init(HINSTANCE hInst)
{
    m_hInstance = hInst;    

    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];
    if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
    {
        PathRemoveFileSpecW(path);
        m_exeDirectoryPath.assign(path);
    }

    m_windowManager = std::make_shared<WindowManager>();
    m_windowManager->Init(hInst);

    m_d3dClass = std::make_shared<D3DClass>();
    m_d3dClass->Init();
}

int Application::Run()
{
    m_tutScene = std::make_shared<Tutorial2>(L"Cube Scene", SCREEN_WIDTH, SCREEN_HEIGHT);

    if (!m_tutScene->Init(m_d3dClass))
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

    m_d3dClass->Flush();

    m_tutScene->UnloadContent();
    m_tutScene->Destroy();

    return static_cast<int>(msg.wParam);
}

wstring Application::GetEXEDirectoryPath()
{
    return m_exeDirectoryPath;
}