#pragma once

#include "pch.h"

#include "Events.h"

#include <memory> // for std::enable_shared_from_this
#include <string> // for std::wstring

#include "Application.h"
#include "Batch.h"

class Window;
class Batch;

class Scene : public std::enable_shared_from_this<Scene>
{
public:
    Scene(const std::wstring& name, int width, int height, bool vSync);
    virtual ~Scene();

    virtual bool Initialize();
    virtual bool LoadContent() = 0;

    virtual void UnloadContent() = 0;
    virtual void Destroy();

protected:
    friend class Window; // HUH?

    virtual void OnUpdate(UpdateEventArgs& e);
    virtual void OnRender(RenderEventArgs& e);

    virtual void OnKeyPressed(KeyEventArgs& e);
    virtual void OnKeyReleased(KeyEventArgs& e);

    virtual void OnMouseMoved(MouseMotionEventArgs& e);
    virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
    virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
    virtual void OnMouseWheel(MouseWheelEventArgs& e);

    virtual void OnResize(ResizeEventArgs& e);
    virtual void OnWindowDestroy();

    std::shared_ptr<Window> m_pWindow;

    int m_Width;
    int m_Height;

    ComPtr<ID3D12Device2> m_device; // WHAT R ZE NUMBARS

private:
    std::wstring m_Name;
    bool m_vSync;    

};