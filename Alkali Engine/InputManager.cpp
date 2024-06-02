#include "pch.h"
#include "InputManager.h"

unordered_map<KeyCode::Key, bool> InputManager::s_currentlyHeldKeys;
unordered_map<KeyCode::Key, bool> InputManager::s_keysLastFrame;

XMFLOAT2 InputManager::m_mousePos;
XMFLOAT2 InputManager::m_mousePosLastFrame;

XMFLOAT2 InputManager::m_mousePosClient;
XMFLOAT2 InputManager::m_mousePosClientLastFrame;

float InputManager::m_mouseWheelDelta;

bool InputManager::m_mouseLeftState;
bool InputManager::m_mouseRightState;
bool InputManager::m_mouseMiddleState;

bool InputManager::m_mouseLeftStateLastFrame;
bool InputManager::m_mouseRightStateLastFrame;
bool InputManager::m_mouseMiddleStateLastFrame;

void InputManager::AddKey(KeyCode::Key key)
{
    s_currentlyHeldKeys.insert_or_assign(key, true);
}

void InputManager::RemoveKey(KeyCode::Key key)
{
    s_currentlyHeldKeys.insert_or_assign(key, false);
}

void InputManager::SetMouseLeftState(bool state)
{
    m_mouseLeftState = state;
}

void InputManager::SetMouseRightState(bool state)
{
    m_mouseRightState = state;
}

void InputManager::SetMouseMiddleState(bool state)
{
    m_mouseMiddleState = state;
}

void InputManager::SetMousePos(XMFLOAT2 pos)
{
    m_mousePos = pos;
}

void InputManager::SetMousePosClient(XMFLOAT2 pos)
{
    m_mousePosClient = pos;
}

void InputManager::SetMouseWheelDelta(float delta)
{
    m_mouseWheelDelta += delta;
}

bool InputManager::KeyActive(unordered_map<KeyCode::Key, bool> map, KeyCode::Key key) 
{
    auto iter = map.find(key);
    if (iter == map.end())
        return false;

    return map.at(key);
}

bool InputManager::IsKeyDown(KeyCode::Key code)
{
    return KeyActive(s_currentlyHeldKeys, code) && !KeyActive(s_keysLastFrame, code);
}

bool InputManager::IsKey(KeyCode::Key code)
{
    return KeyActive(s_currentlyHeldKeys, code);
}

bool InputManager::IsKeyUp(KeyCode::Key code)
{
    return !KeyActive(s_currentlyHeldKeys, code) && KeyActive(s_keysLastFrame, code);
}

bool InputManager::IsShiftHeld()
{
    return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

bool InputManager::IsCtrlHeld()
{
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool InputManager::IsAltHeld()
{
    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}

bool InputManager::IsMouseLeftDown()
{
    return m_mouseLeftState && !m_mouseLeftStateLastFrame;
}

bool InputManager::IsMouseLeft()
{
    return m_mouseLeftState;
}

bool InputManager::IsMouseLeftUp()
{
    return !m_mouseLeftState && m_mouseLeftStateLastFrame;
}

bool InputManager::IsMouseRightDown()
{
    return m_mouseRightState && !m_mouseRightStateLastFrame;
}

bool InputManager::IsMouseRight()
{
    return m_mouseRightState;
}

bool InputManager::IsMouseRightUp()
{
    return !m_mouseRightState && m_mouseRightStateLastFrame;
}

bool InputManager::IsMouseMiddleDown()
{
    return m_mouseMiddleState && !m_mouseMiddleStateLastFrame;
}

bool InputManager::IsMouseMiddle()
{
    return m_mouseMiddleState;
}

bool InputManager::IsMouseMiddleUp()
{
    return !m_mouseMiddleState && m_mouseMiddleStateLastFrame;
}

XMFLOAT2 InputManager::GetMousePos()
{
    return m_mousePos;
}

XMFLOAT2 InputManager::GetMousePosDelta()
{
    return XMFLOAT2(m_mousePos.x - m_mousePosLastFrame.x, m_mousePos.y - m_mousePosLastFrame.y);
}

XMFLOAT2 InputManager::GetMousePosClient()
{
    return m_mousePosClient;
}

XMFLOAT2 InputManager::GetMousePosClientDelta()
{
    return XMFLOAT2(m_mousePosClient.x - m_mousePosClientLastFrame.x, m_mousePosClient.y - m_mousePosClientLastFrame.y);
}

float InputManager::GetMouseWheelDelta()
{
    return m_mouseWheelDelta;
}

void InputManager::ProgressFrame()
{
    s_keysLastFrame = s_currentlyHeldKeys;

    m_mouseLeftStateLastFrame = m_mouseLeftState;
    m_mouseRightStateLastFrame = m_mouseRightState;
    m_mouseMiddleStateLastFrame = m_mouseMiddleState;

    m_mousePosLastFrame = m_mousePos;
    m_mousePosClientLastFrame = m_mousePosClient;

    m_mouseWheelDelta = 0;
}
