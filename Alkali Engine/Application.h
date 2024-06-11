#pragma once

#include "pch.h"
#include "Settings.h"
#include "D3DClass.h"
#include "WindowManager.h"

#include <memory>
#include <string>
#include <unordered_map>

class Tutorial2;
class Scene;

using std::wstring;
using std::shared_ptr;
using std::unordered_map;

class Application
{
public:
    Application(HINSTANCE hInst);

    void InitScene(shared_ptr<Scene> scene);

    int Run();

    void RenderImGuiScenes();

    void CalculateFPS(float deltaTime);

    void Shutdown();

    static wstring GetEXEDirectoryPath();

    void ChangeScene(wstring sceneID);

    void AssignScene(shared_ptr<Scene> scene);

    void DestroyScenes();

private:        
    HINSTANCE m_hInstance;

    shared_ptr<D3DClass> m_d3dClass;
    shared_ptr<WindowManager> m_windowManager;
    shared_ptr<Window> m_mainWindow;

    HighResolutionClock m_updateClock;
    HighResolutionClock m_renderClock;

    shared_ptr<Scene> m_currentScene;

    unordered_map<wstring, shared_ptr<Scene>> m_sceneMap;

    static wstring ms_exeDirectoryPath;

    double m_fps;
    double m_fpsTimeSinceUpdate;
    unsigned int m_fpsFramesSinceUpdate;
};