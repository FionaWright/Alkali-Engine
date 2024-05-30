#pragma once

#include "pch.h"
#include "Settings.h"
#include "D3DClass.h"
#include "WindowManager.h"

#include <memory>
#include <string>

class Tutorial2;

using std::wstring;
using std::shared_ptr;
using std::map;

class Application
{
public:
    Application();
    virtual ~Application();

    void Init(HINSTANCE hInst);

    int Run();

    wstring GetEXEDirectoryPath();

private:        
    HINSTANCE m_hInstance;

    shared_ptr<D3DClass> m_d3dClass;

    shared_ptr<WindowManager> m_windowManager;

    shared_ptr<Tutorial2> m_tutScene;

    wstring m_exeDirectoryPath;
};