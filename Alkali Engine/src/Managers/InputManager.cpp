#include "pch.h"
#include "InputManager.h"

unordered_map<KeyCode::Key, bool> InputManager::ms_currentlyHeldKeys;
unordered_map<KeyCode::Key, bool> InputManager::ms_keysLastFrame;

XMFLOAT2 InputManager::ms_mousePos;
XMFLOAT2 InputManager::ms_mousePosLastFrame;

XMFLOAT2 InputManager::ms_mousePosClient;
XMFLOAT2 InputManager::ms_mousePosClientLastFrame;

float InputManager::ms_mouseWheelDelta;

bool InputManager::ms_mouseLeftState;
bool InputManager::ms_mouseRightState;
bool InputManager::ms_mouseMiddleState;

bool InputManager::ms_mouseLeftStateLastFrame;
bool InputManager::ms_mouseRightStateLastFrame;
bool InputManager::ms_mouseMiddleStateLastFrame;

void InputManager::AddKey(KeyCode::Key key)
{
    ms_currentlyHeldKeys.insert_or_assign(key, true);
}

void InputManager::RemoveKey(KeyCode::Key key)
{
    ms_currentlyHeldKeys.insert_or_assign(key, false);
}

void InputManager::SetMouseLeftState(bool state)
{
    ms_mouseLeftState = state;
}

void InputManager::SetMouseRightState(bool state)
{
    ms_mouseRightState = state;
}

void InputManager::SetMouseMiddleState(bool state)
{
    ms_mouseMiddleState = state;
}

void InputManager::SetMousePos(XMFLOAT2 pos)
{
    ms_mousePos = pos;
}

void InputManager::SetMousePosClient(XMFLOAT2 pos)
{
    ms_mousePosClient = pos;
}

void InputManager::SetMouseWheelDelta(float delta)
{
    ms_mouseWheelDelta += delta;
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
    return KeyActive(ms_currentlyHeldKeys, code) && !KeyActive(ms_keysLastFrame, code);
}

bool InputManager::IsKey(KeyCode::Key code)
{
    return KeyActive(ms_currentlyHeldKeys, code);
}

bool InputManager::IsKeyUp(KeyCode::Key code)
{
    return !KeyActive(ms_currentlyHeldKeys, code) && KeyActive(ms_keysLastFrame, code);
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
    return ms_mouseLeftState && !ms_mouseLeftStateLastFrame;
}

bool InputManager::IsMouseLeft()
{
    return ms_mouseLeftState;
}

bool InputManager::IsMouseLeftUp()
{
    return !ms_mouseLeftState && ms_mouseLeftStateLastFrame;
}

bool InputManager::IsMouseRightDown()
{
    return ms_mouseRightState && !ms_mouseRightStateLastFrame;
}

bool InputManager::IsMouseRight()
{
    return ms_mouseRightState;
}

bool InputManager::IsMouseRightUp()
{
    return !ms_mouseRightState && ms_mouseRightStateLastFrame;
}

bool InputManager::IsMouseMiddleDown()
{
    return ms_mouseMiddleState && !ms_mouseMiddleStateLastFrame;
}

bool InputManager::IsMouseMiddle()
{
    return ms_mouseMiddleState;
}

bool InputManager::IsMouseMiddleUp()
{
    return !ms_mouseMiddleState && ms_mouseMiddleStateLastFrame;
}

XMFLOAT2 InputManager::GetMousePos()
{
    return ms_mousePos;
}

XMFLOAT2 InputManager::GetMousePosDelta()
{
    return XMFLOAT2(ms_mousePos.x - ms_mousePosLastFrame.x, ms_mousePos.y - ms_mousePosLastFrame.y);
}

XMFLOAT2 InputManager::GetMousePosClient()
{
    return ms_mousePosClient;
}

XMFLOAT2 InputManager::GetMousePosClientDelta()
{
    return XMFLOAT2(ms_mousePosClient.x - ms_mousePosClientLastFrame.x, ms_mousePosClient.y - ms_mousePosClientLastFrame.y);
}

float InputManager::GetMouseWheelDelta()
{
    return ms_mouseWheelDelta;
}

void InputManager::ProgressFrame()
{
    ms_keysLastFrame = ms_currentlyHeldKeys;

    ms_mouseLeftStateLastFrame = ms_mouseLeftState;
    ms_mouseRightStateLastFrame = ms_mouseRightState;
    ms_mouseMiddleStateLastFrame = ms_mouseMiddleState;

    ms_mousePosLastFrame = ms_mousePos;
    ms_mousePosClientLastFrame = ms_mousePosClient;

    ms_mouseWheelDelta = 0;
}
