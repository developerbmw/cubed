#ifndef CUBED_WINDOW_H
#define CUBED_WINDOW_H

#include <functional>
#include <sdl2/include/SDL.h>
#include <string>
#include <utility>
#include <vector>

class InputManager;

class Window
{
public:
	Window(const std::string& title, int width, int height, InputManager& input);
	~Window();
	Window(const Window&) = delete;

	void update(bool mouse_input);
	void swap_buffers();
	void add_resize_handler(std::function<void(const std::pair<int, int>&)> handler) { m_window_resize_handlers.emplace_back(std::move(handler)); }
	void center_mouse() { SDL_WarpMouseInWindow(m_window, m_window_center.first, m_window_center.second); }

	const std::pair<int, int>& get_window_size() const { return m_window_size; }

private:
	SDL_Window* m_window;
	SDL_GLContext m_context;
	InputManager& m_input;
	std::pair<int, int> m_window_size;
	std::pair<int, int> m_window_center;
	std::vector<std::function<void(const std::pair<int, int>&)>> m_window_resize_handlers;
};

#include "cubed_exception.h"

class WindowException : public CubedException
{
public:
	WindowException(std::string message) : CubedException(std::move(message)) { }
};

#endif