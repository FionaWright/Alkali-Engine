#include "pch.h"
#include "Application.h"
#include <Shlwapi.h>

#include "Window.h"
#include "CommandQueue.h"
#include "Scene.h"
#include "Tutorial2.h"
#include "ModelLoader.h"
#include "ImGUIManager.h"

Application::Application(HINSTANCE hInst)
    : m_hInstance(hInst)
    , m_windowManager(std::make_shared<WindowManager>())
    , m_d3dClass(std::make_shared<D3DClass>())
{
    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];
    if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
    {
        PathRemoveFileSpecW(path);
        m_exeDirectoryPath.assign(path);
    }

    m_windowManager->Init(hInst);
    m_d3dClass->Init();

    bool vSync = false;
    m_mainWindow = WindowManager::GetInstance()->CreateRenderWindow(m_d3dClass, L"Main Window", SCREEN_WIDTH, SCREEN_HEIGHT, vSync);
    m_mainWindow->Show();

    shared_ptr<Tutorial2> tut2Scene = std::make_shared<Tutorial2>(L"Cube Scene", m_mainWindow);
    if (!tut2Scene->Init(m_d3dClass))
        throw new std::exception("Failed to initialise base scene");
    m_sceneMap.emplace(tut2Scene->m_Name, tut2Scene);

    AssignScene(tut2Scene);

    ImGUIManager::Init(m_mainWindow->GetHWND(), m_d3dClass->GetDevice(), BACK_BUFFER_COUNT, SWAP_CHAIN_DXGI_FORMAT);

    //ModelLoader::PreprocessObjFile(L"C:\\Users\\finnw\\OneDrive\\Documents\\3D objects\\Madeline.obj");
}

int Application::Run()
{   
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        ImGUIManager::Begin();

        if (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        else
        {
            m_mainWindow->OnUpdate();
            InputManager::ProgressFrame();
        }
               
        m_mainWindow->OnRender();        
    }    

    return static_cast<int>(msg.wParam);
}

void Application::Shutdown()
{
    m_d3dClass->Flush();

    ImGUIManager::Shutdown();
    
    ResourceManager::Shutdown();

    DestroyScenes();
    m_mainWindow->Destroy();
    m_sceneMap.clear();
}

wstring Application::GetEXEDirectoryPath()
{
    return m_exeDirectoryPath;
}

void Application::ChangeScene(wstring sceneID)
{
    shared_ptr<Scene> scene = m_sceneMap.at(sceneID);

    AssignScene(scene);
}

void Application::AssignScene(shared_ptr<Scene> scene) 
{
    if (m_currentScene && m_currentScene->m_ContentLoaded)
    {
        m_d3dClass->Flush();
        m_currentScene->UnloadContent();
        m_currentScene->m_ContentLoaded = false;
    }

    if (!scene->m_ContentLoaded)
    {
        if (!scene->LoadContent())
            throw new std::exception("Failed to load content of base scene");
    }

    m_mainWindow->RegisterCallbacks(scene);    

    scene->m_ContentLoaded = true;

    m_currentScene = scene;
}

void Application::DestroyScenes() 
{
    for (const auto& pair : m_sceneMap)
    {
        shared_ptr<Scene> scene = pair.second;
        
        scene->UnloadContent();
        scene->Destroy();
    }
}