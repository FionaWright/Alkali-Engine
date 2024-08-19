#include "pch.h"
#include "Application.h"
#include <Shlwapi.h>

#include "Window.h"
#include "CommandQueue.h"
#include "Scene.h"
#include "SceneTest.h"
#include "SceneTrueEmpty.h"
#include "ImGUIManager.h"
#include "Utils.h"
#include "SceneBistro.h"
#include "TextureLoader.h"
#include "ResourceTracker.h"
#include "DescriptorManager.h"
#include "AssetFactory.h"
#include "AlkaliGUIManager.h"
#include "ShadowManager.h"
#include <SceneChess.h>
#include "SceneCube.h"
#include <LoadManager.h>

wstring Application::ms_exeDirectoryPath;

Application::Application(HINSTANCE hInst)
    : m_hInstance(hInst)
    , m_windowManager(std::make_unique<WindowManager>())
    , m_d3dClass(std::make_unique<D3DClass>())
{
    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];
    if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
    {
        PathRemoveFileSpecW(path);
        ms_exeDirectoryPath.assign(path);
    }

    m_windowManager->Init(hInst);
    m_d3dClass->Init();    

    AssetFactory::Init(m_d3dClass.get());

    bool vSync = false;
    m_mainWindow = WindowManager::GetInstance()->CreateRenderWindow(m_d3dClass.get(), L"Alkali Engine", static_cast<int>(SettingsManager::ms_Window.ScreenWidth), static_cast<int>(SettingsManager::ms_Window.ScreenHeight), vSync);
    m_mainWindow->Show();
    
    DescriptorManager::Init(m_d3dClass.get(), SettingsManager::ms_DX12.DescriptorHeapSize);

    ImGUIManager::Init(m_mainWindow->GetHWND(), m_d3dClass->GetDevice(), BACK_BUFFER_COUNT);

    if (SettingsManager::ms_DX12.DebugLoadSingleSceneOnly)
    {
        auto trueEmptyScene = std::make_shared<SceneTrueEmpty>(L"True Empty Scene", m_mainWindow.get());
        InitScene(trueEmptyScene);
        AssignScene(trueEmptyScene.get());
        return;
    }

    auto testScene = std::make_shared<SceneTest>(L"Test Scene", m_mainWindow.get());
    InitScene(testScene);

    auto bistroScene = std::make_shared<SceneBistro>(L"Bistro Scene", m_mainWindow.get());
    InitScene(bistroScene);

    auto chessScene = std::make_shared<SceneChess>(L"Chess Scene", m_mainWindow.get());
    InitScene(chessScene);

    auto emptyScene = std::make_shared<Scene>(L"Empty Scene", m_mainWindow.get(), true);
    InitScene(emptyScene);

    auto trueEmptyScene = std::make_shared<SceneTrueEmpty>(L"True Empty Scene", m_mainWindow.get());
    InitScene(trueEmptyScene);

    auto cubeScene = std::make_shared<SceneCube>(L"Cube Scene", m_mainWindow.get());
    InitScene(cubeScene);

    AssignScene(emptyScene.get());
}

void Application::InitScene(shared_ptr<Scene> scene)
{
    if (!scene->Init(m_d3dClass.get()))
        throw new std::exception("Failed to initialise scene");

    m_sceneMap.emplace(scene->m_Name, scene);
}

int Application::Run()
{   
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        ImGUIManager::Begin();

        AlkaliGUIManager::RenderGUI(m_d3dClass.get(), m_currentScene, this);

        if (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        m_updateClock.Tick();
        TimeEventArgs argsU = m_updateClock.GetTimeArgs();
        CalculateFPS(argsU.ElapsedTime);

        argsU.ElapsedTime *= SettingsManager::ms_Dynamic.UpdateTimeScale;
        argsU.TotalTime *= SettingsManager::ms_Dynamic.UpdateTimeScale;

        m_mainWindow->OnUpdate(argsU);
        InputManager::ProgressFrame();

        m_hotReloadCounter++;
        if (m_hotReloadCounter >= SettingsManager::ms_Misc.HotReloadTimeSlice)
        {
            TryHotReloadShaders();
            m_hotReloadCounter = 0;
        }
               
        m_renderClock.Tick();
        TimeEventArgs args = m_renderClock.GetTimeArgs();
        m_mainWindow->OnRender(args);        
    }        

    return static_cast<int>(msg.wParam);
}

void Application::CalculateFPS(double deltaTime) 
{
    m_fpsTimeSinceUpdate += deltaTime;
    m_fpsFramesSinceUpdate++;

    if (m_fpsTimeSinceUpdate > 0.1)
    {
        m_fps = m_fpsFramesSinceUpdate / m_fpsTimeSinceUpdate;

        m_fpsFramesSinceUpdate = 0;
        m_fpsTimeSinceUpdate = 0.0;
    }
}

void Application::Shutdown()
{
    m_d3dClass->Flush();

    ImGUIManager::Shutdown();   
    TextureLoader::Shutdown();
    ResourceTracker::ReleaseAll();
    DescriptorManager::Shutdown();
    Scene::StaticShutdown();
    ShadowManager::Shutdown();
    ResourceManager::Shutdown();

    DestroyScenes();    
    m_mainWindow->Destroy();
    m_windowManager.reset();
    m_mainWindow.reset();
    m_sceneMap.clear();
    m_d3dClass.reset();    
}

wstring Application::GetEXEDirectoryPath()
{
    return ms_exeDirectoryPath;
}

void Application::ChangeScene(wstring sceneID)
{
    shared_ptr<Scene> scene = m_sceneMap.at(sceneID);

    AssignScene(scene.get());
}

void Application::AssignScene(Scene* scene)
{
    auto startTimeProfiler = std::chrono::high_resolution_clock::now();

    m_d3dClass->Flush();

    if (m_currentScene && m_currentScene->m_ContentLoaded)
    {
        m_currentScene->UnloadContent();
        m_currentScene->m_ContentLoaded = false;
    }

    m_updateClock.Reset();
    m_renderClock.Reset();

    LoadManager::StartLoading(m_d3dClass.get(), SettingsManager::ms_DX12.AsyncLoadingThreadCount);

    if (!scene->m_ContentLoaded)
    {
        if (!scene->LoadContent())
            throw new std::exception("Failed to load content of base scene");
    }

    LoadManager::StopOnFlush();

    ResizeEventArgs e = { m_mainWindow->GetClientWidth(), m_mainWindow->GetClientHeight() };
    scene->OnResize(e);

    m_mainWindow->RegisterCallbacks(scene);    

    scene->m_ContentLoaded = true;
    m_currentScene = scene;

    std::chrono::duration<double, std::milli> timeTaken = std::chrono::high_resolution_clock::now() - startTimeProfiler;
    wstring output = L"Time taken to load " + m_currentScene->m_Name + L" was " + std::to_wstring(timeTaken.count()) + L" ms";
    OutputDebugStringW(output.c_str());
}

void Application::DestroyScenes() 
{
    for (const auto& pair : m_sceneMap)
    {
        shared_ptr<Scene> scene = pair.second;
        
        scene->UnloadContent();
        scene->Destroy();
        scene.reset();
    }
}

double Application::GetFPS()
{
    return m_fps;
}

unordered_map<wstring, shared_ptr<Scene>>& Application::GetSceneMap()
{
    return m_sceneMap;
}

void Application::TryHotReloadShaders()
{
    auto& shaders = ResourceTracker::GetShaders();
    for (auto& it : shaders)
    {
        it.second->TryHotReload(m_d3dClass->GetDevice());
    }
}
