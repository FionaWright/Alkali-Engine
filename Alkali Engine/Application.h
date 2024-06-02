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

    int Run();

    void Shutdown();

    wstring GetEXEDirectoryPath();

private:        
    HINSTANCE m_hInstance;

    shared_ptr<D3DClass> m_d3dClass;

    shared_ptr<WindowManager> m_windowManager;

    shared_ptr<Tutorial2> m_tutScene;

    unordered_map<wstring, Scene*> m_sceneMap;

    wstring m_exeDirectoryPath;
};