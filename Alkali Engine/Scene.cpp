#include "pch.h"
#include "Scene.h"

Scene::Scene(const std::wstring& name, int width, int height, bool vSync)
{
    m_Width = width;
    m_Height = height;
    m_vSync = vSync;
    m_Name = name;
}

Scene::~Scene()
{
}

bool Scene::Initialize()
{
    // Check for DirectX Math library support.
    if (!DirectX::XMVerifyCPUSupport())
    {
        MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    m_pWindow = Application::Get().CreateRenderWindow(m_Name, m_Width, m_Height, m_vSync);
    m_pWindow->RegisterCallbacks(shared_from_this());
    m_pWindow->Show();

    return true;
}

bool Scene::LoadContent()
{
    return false;
}

void Scene::UnloadContent()
{
}

void Scene::Destroy()
{
    Application::Get().DestroyWindow(m_pWindow);
    m_pWindow.reset();
}

void Scene::OnUpdate(UpdateEventArgs& e)
{
}

void Scene::OnRender(RenderEventArgs& e)
{
}

void Scene::OnKeyPressed(KeyEventArgs& e)
{
}

void Scene::OnKeyReleased(KeyEventArgs& e)
{
}

void Scene::OnMouseMoved(MouseMotionEventArgs& e)
{
}

void Scene::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
}

void Scene::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
}

void Scene::OnMouseWheel(MouseWheelEventArgs& e)
{
}

void Scene::OnResize(ResizeEventArgs& e)
{
}

void Scene::OnWindowDestroy()
{
}

int Scene::GetWidth()
{
    return m_Width;
}

int Scene::GetHeight()
{
    return m_Height;
}
