#pragma once

#include "pch.h"
#include "Constants.h"
#include "SettingsManager.h"
#include "D3DClass.h"
#include "WindowManager.h"

#include <memory>
#include <string>
#include <unordered_map>

class SceneTest;
class Scene;

using std::wstring;
using std::shared_ptr;
using std::unordered_map;
using std::unique_ptr;

class Application
{
public:
    Application(HINSTANCE hInst);

    void InitScene(shared_ptr<Scene> scene);

    int Run();

    void CalculateFPS(double deltaTime);

    void Shutdown();

    static wstring GetEXEDirectoryPath();

    void ChangeScene(wstring sceneID);

    void AssignScene(Scene* scene);

    void DestroyScenes();

    double GetFPS();
    double GetFPS_2();
    double GetFPS_3();
    unordered_map<wstring, shared_ptr<Scene>>& GetSceneMap();

private:        
    void TryHotReloadShaders();

    HINSTANCE m_hInstance;

    unique_ptr<D3DClass> m_d3dClass;
    unique_ptr<WindowManager> m_windowManager;
    shared_ptr<Window> m_mainWindow;

    HighResolutionClock m_updateClock;
    HighResolutionClock m_renderClock;

    Scene* m_currentScene;

    unordered_map<wstring, shared_ptr<Scene>> m_sceneMap;

    static wstring ms_exeDirectoryPath;

    double m_fps, m_fps2, m_fps3;
    double m_fpsTimeSinceUpdate, m_fpsTimeSinceUpdate2, m_fpsTimeSinceUpdate3;
    unsigned int m_fpsFramesSinceUpdate, m_fpsFramesSinceUpdate2, m_fpsFramesSinceUpdate3;
    int m_hotReloadCounter = 0;
};