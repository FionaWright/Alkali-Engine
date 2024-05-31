#include "pch.h"
#include "InputManager.h"

unordered_map<KeyCode::Key, bool> InputManager::s_currentlyHeldKeys;
unordered_map<KeyCode::Key, bool> InputManager::s_keysLastFrame;

void InputManager::AddKey(KeyState keyState)
{
    s_currentlyHeldKeys.insert_or_assign(keyState.KeyCode, true);
}

void InputManager::RemoveKey(KeyState keyState)
{
    s_currentlyHeldKeys.insert_or_assign(keyState.KeyCode, false);
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

void InputManager::ProgressFrame()
{
    s_keysLastFrame = s_currentlyHeldKeys;
}
