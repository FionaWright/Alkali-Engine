#include "pch.h"
#include "Application.h"
#include <Shlwapi.h>

#include "Window.h"
#include "CommandQueue.h"
#include "Scene.h"
#include "Tutorial2.h"
#include "ModelLoader.h"
#include "ImGUIManager.h"
#include "Utils.h"
#include "CubeScene.h"

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

    shared_ptr<Tutorial2> tut2Scene = std::make_shared<Tutorial2>(L"Madeline Scene", m_mainWindow);
    InitScene(tut2Scene);

    shared_ptr<CubeScene> cubeScene = std::make_shared<CubeScene>(L"Cube Scene", m_mainWindow);
    InitScene(cubeScene);

    AssignScene(tut2Scene);

    ImGUIManager::Init(m_mainWindow->GetHWND(), m_d3dClass->GetDevice(), BACK_BUFFER_COUNT, SWAP_CHAIN_DXGI_FORMAT);

    //ModelLoader::PreprocessObjFile(L"C:\\Users\\finnw\\OneDrive\\Documents\\3D objects\\Madeline.obj");
}

void Application::InitScene(shared_ptr<Scene> scene)
{
    if (!scene->Init(m_d3dClass))
        throw new std::exception("Failed to initialise scene");
    m_sceneMap.emplace(scene->m_Name, scene);
}

int Application::Run()
{   
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        ImGUIManager::Begin();

        RenderImGuiScenes();

        if (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        else
        {
            m_updateClock.Tick();
            TimeEventArgs args = m_updateClock.GetTimeArgs();
            m_mainWindow->OnUpdate(args);
            InputManager::ProgressFrame();
        }
               
        m_renderClock.Tick();
        TimeEventArgs args = m_renderClock.GetTimeArgs();
        m_mainWindow->OnRender(args);        
    }    

    return static_cast<int>(msg.wParam);
}

void Application::RenderImGuiScenes() 
{
    if (ImGui::CollapsingHeader("Scenes"))
    {
        for (const auto& pair : m_sceneMap)
        {
            bool disabled = pair.second == m_currentScene;
            if (disabled)
                ImGui::BeginDisabled(true);

            if (ImGui::Button(wstringToString(pair.first).c_str()))
            {
                AssignScene(pair.second);
            }

            if (disabled)
                ImGui::EndDisabled();
        }

        if (ImGui::Button("Reset current scene"))
        {
            AssignScene(m_currentScene);
        }
    }
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
    m_d3dClass->Flush();

    if (m_currentScene && m_currentScene->m_ContentLoaded)
    {        
        m_currentScene->UnloadContent();
        m_currentScene->m_ContentLoaded = false;
    }

    m_updateClock.Reset();
    m_renderClock.Reset();

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