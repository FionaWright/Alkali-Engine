#pragma once

#include "Keycodes.h"
#include <unordered_map>

using std::unordered_map;

struct KeyState
{
	KeyCode::Key KeyCode;
	unsigned int UnicodeChar;
};

class InputManager
{
public:
	static void AddKey(KeyState keyState);
	static void RemoveKey(KeyState keyState);

	static bool KeyActive(unordered_map<KeyCode::Key, bool> map, KeyCode::Key key);

	static bool IsKeyDown(KeyCode::Key code);
	static bool IsKey(KeyCode::Key code);
	static bool IsKeyUp(KeyCode::Key code);

	static bool IsShiftHeld();
	static bool IsCtrlHeld();
	static bool IsAltHeld();

	static void ProgressFrame();

private:
	static unordered_map<KeyCode::Key, bool> s_currentlyHeldKeys;
	static unordered_map<KeyCode::Key, bool> s_keysLastFrame;
};

