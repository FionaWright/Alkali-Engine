#include "pch.h"

#include "Application.h"

#include <Shlwapi.h>
#include <dxgidebug.h>

using std::unique_ptr;

void ReportLiveObjects()
{
    IDXGIDebug1* dxgiDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    dxgiDebug->Release();
}

_Use_decl_annotations_ int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    int retCode = 0;

    Application app(hInstance);

    retCode = app.Run();

    app.Shutdown();

    if (SettingsManager::ms_DX12.DebugReportLiveObjects)
        atexit(&ReportLiveObjects);

    return retCode;
}