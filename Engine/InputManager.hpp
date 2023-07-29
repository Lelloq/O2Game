#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <SDL2/SDL.h>

#include "Data/WindowsTypes.hpp"
#include "Keys.h"
#include "InputEvent.hpp"

typedef std::function<void(const KeyState&)> key_event_callback;
typedef std::function<void(const MouseState&)> mouse_event_callback;

class InputManager {
public:
	void Update(SDL_Event& event);

	bool IsKeyDown(Keys key);
	bool IsMouseButton(MouseButton button);

	void ListenKeyEvent(key_event_callback callback);
	void ListenMouseEvent(mouse_event_callback callback);

	Rect GetMousePosition();

	static InputManager* GetInstance();
	static void Release();
private:
	InputManager();
	~InputManager();

	static InputManager* s_instance;

	void OnKeyEvent(SDL_Event& event, bool isDown);
	void OnMouseEvent(SDL_MouseButtonEvent& event, bool isDown);
	void OnMousePositionEvent(SDL_MouseMotionEvent& event);

	Rect m_mousePosition;
	std::unordered_map<Keys, bool> m_keyStates;
	std::unordered_map<MouseButton, bool> m_mouseButton;

	std::vector<key_event_callback> m_keyEventCallbacks;
	std::vector<mouse_event_callback> m_mouseEventCallbacks;
};