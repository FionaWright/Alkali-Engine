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
steady_clock::time_point Application::m_timePointFromStartOfProgram;

Application::Application(HINSTANCE hInst)
    : m_hInstance(hInst)
    , m_windowManager(std::make_unique<WindowManager>())
    , m_d3dClass(std::make_unique<D3DClass>())
{
    m_timePointFromStartOfProgram = std::chrono::high_resolution_clock::now();
    AlkaliGUIManager::EnableCombinedLog(true);

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
    TextureLoader::InitComputeShaders(m_d3dClass->GetDevice());

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

    AssignScene(bistroScene.get());
}

void Application::InitScene(shared_ptr<Scene> scene)
{
    if (!scene->Init(m_d3dClass.get()))
        throw new std::exception("Failed to initialise scene");

    AlkaliGUIManager::LogUntaggedMessage("Scene Initialised: " + wstringToString(scene->m_Name));

    m_sceneMap.emplace(scene->m_Name, scene);
}

int Application::Run()
{   
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        AlkaliGUIManager::LogUntaggedMessage("Beginning Frame");

        ImGUIManager::Begin();
        AlkaliGUIManager::RenderGUI(m_d3dClass.get(), m_currentScene, this);
        AlkaliGUIManager::LogUntaggedMessage("Rendered GUI");

        if (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        AlkaliGUIManager::LogUntaggedMessage("Dealt with WIN Messages");

        m_updateClock.Tick();
        TimeEventArgs argsU = m_updateClock.GetTimeArgs();
        CalculateFPS(argsU.ElapsedTime);

        argsU.ElapsedTime *= SettingsManager::ms_Dynamic.UpdateTimeScale;
        argsU.TotalTime *= SettingsManager::ms_Dynamic.UpdateTimeScale;

        m_mainWindow->OnUpdate(argsU);
        AlkaliGUIManager::LogUntaggedMessage("Update Tick");
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

        if (m_totalFramesElapsed == 10 && SettingsManager::ms_Misc.LogDisableCombinedAfter10Frames)
        {
            AlkaliGUIManager::LogUntaggedMessage("10 Frames Elapsed, Disabling Combined Log");
            AlkaliGUIManager::EnableCombinedLog(false);
        }

        AlkaliGUIManager::LogUntaggedMessage("Frame Tick");
        m_totalFramesElapsed++;
    }        

    return static_cast<int>(msg.wParam);
}

void Application::CalculateFPS(double deltaTime) 
{
    m_fpsTimeSinceUpdate += deltaTime;
    m_fpsTimeSinceUpdate2 += deltaTime;
    m_fpsTimeSinceUpdate3 += deltaTime;
    m_fpsFramesSinceUpdate++;
    m_fpsFramesSinceUpdate2++;
    m_fpsFramesSinceUpdate3++;

    if (m_fpsTimeSinceUpdate > 0.1)
    {
        m_fps = m_fpsFramesSinceUpdate / m_fpsTimeSinceUpdate;

        m_fpsFramesSinceUpdate = 0;
        m_fpsTimeSinceUpdate = 0.0;
    }

    if (m_fpsTimeSinceUpdate2 > 0.5)
    {
        m_fps2 = m_fpsFramesSinceUpdate2 / m_fpsTimeSinceUpdate2;

        m_fpsFramesSinceUpdate2 = 0;
        m_fpsTimeSinceUpdate2 = 0.0;
    }

    if (m_fpsTimeSinceUpdate3 > 1.0)
    {
        m_fps3 = m_fpsFramesSinceUpdate3 / m_fpsTimeSinceUpdate3;

        m_fpsFramesSinceUpdate3 = 0;
        m_fpsTimeSinceUpdate3 = 0.0;
    }
}

void Application::Shutdown()
{
    LoadManager::FullShutdown();

    if (m_d3dClass)
        m_d3dClass->Flush();
    
    ImGUIManager::Shutdown();   
    TextureLoader::Shutdown();
    ResourceTracker::ReleaseAll();
    DescriptorManager::Shutdown();
    Scene::StaticShutdown();
    ShadowManager::Shutdown();
    ResourceManager::Shutdown();    

    DestroyScenes();    
    if (m_mainWindow)
    {
        m_mainWindow->Destroy();
        m_mainWindow.reset();
    }        

    if (m_windowManager)
        m_windowManager.reset();    

    m_sceneMap.clear();
    if (m_d3dClass)
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

    ResourceTracker::ClearShaderList();

    LoadManager::StartLoading(m_d3dClass.get(), SettingsManager::ms_DX12.Async.ThreadCount);    

    if (!scene->m_ContentLoaded)
    {
        if (!scene->LoadContent())
            throw new std::exception("Failed to load content of base scene");
    }

    LoadManager::EnableStopOnFlush();

    ResizeEventArgs e = { m_mainWindow->GetClientWidth(), m_mainWindow->GetClientHeight() };
    scene->OnResize(e);

    m_mainWindow->RegisterCallbacks(scene);    

    scene->m_ContentLoaded = true;
    m_currentScene = scene;

    std::chrono::duration<double, std::milli> timeTaken = std::chrono::high_resolution_clock::now() - startTimeProfiler;
    wstring output = L"Time taken to load " + m_currentScene->m_Name + L" was " + std::to_wstring(timeTaken.count()) + L" ms\n";
    OutputDebugStringW(output.c_str());

    AlkaliGUIManager::LogUntaggedMessage("Scene Assigned: " + wstringToString(scene->m_Name));
    AlkaliGUIManager::LogUntaggedMessage(wstringToString(output));
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

double Application::GetFPS_2()
{
    return m_fps2;
}

double Application::GetFPS_3()
{
    return m_fps3;
}

unordered_map<wstring, shared_ptr<Scene>>& Application::GetSceneMap()
{
    return m_sceneMap;
}

steady_clock::time_point Application::GetTimePointStartOfProgram()
{
    return m_timePointFromStartOfProgram;
}

void Application::TryHotReloadShaders()
{
    auto& shaders = ResourceTracker::GetShaders();
    for (auto& it : shaders)
    {
        if (!it.second->IsInitialised())
            continue;

        it.second->TryHotReload(m_d3dClass->GetDevice());
    }
}
