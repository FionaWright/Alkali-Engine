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
	static void SetMouseLeftState(bool state);
	static void SetMouseRightState(bool state);
	static void SetMouseMiddleState(bool state);
	static void SetMousePos(XMFLOAT2 pos);
	static void SetMousePosClient(XMFLOAT2 pos);
	static void SetMouseWheelDelta(float delta);

	static bool KeyActive(unordered_map<KeyCode::Key, bool> map, KeyCode::Key key);

	static bool IsKeyDown(KeyCode::Key code);
	static bool IsKey(KeyCode::Key code);
	static bool IsKeyUp(KeyCode::Key code);

	static bool IsShiftHeld();
	static bool IsCtrlHeld();
	static bool IsAltHeld();

	static bool IsMouseLeftDown();
	static bool IsMouseLeft();
	static bool IsMouseLeftUp();

	static bool IsMouseRightDown();
	static bool IsMouseRight();
	static bool IsMouseRightUp();

	static bool IsMouseMiddleDown();
	static bool IsMouseMiddle();
	static bool IsMouseMiddleUp();

	static XMFLOAT2 GetMousePos();
	static XMFLOAT2 GetMousePosDelta();
	static XMFLOAT2 GetMousePosClient();
	static XMFLOAT2 GetMousePosClientDelta();

	static float GetMouseWheelDelta();

	static void ProgressFrame();

private:
	static unordered_map<KeyCode::Key, bool> s_currentlyHeldKeys;
	static unordered_map<KeyCode::Key, bool> s_keysLastFrame;

	static XMFLOAT2 m_mousePos;
	static XMFLOAT2 m_mousePosLastFrame;

	static XMFLOAT2 m_mousePosClient;
	static XMFLOAT2 m_mousePosClientLastFrame;

	static float m_mouseWheelDelta;

	static bool m_mouseLeftState;
	static bool m_mouseRightState;
	static bool m_mouseMiddleState;

	static bool m_mouseLeftStateLastFrame;
	static bool m_mouseRightStateLastFrame;
	static bool m_mouseMiddleStateLastFrame;
};

