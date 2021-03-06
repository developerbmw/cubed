#include "input_manager.h"
#include "window.h"

Window::Window(const std::string& title, int width, int height, InputManager& input) :
	m_input(input)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		throw WindowException("SDL_Init() failed");
	}

	m_window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if(!m_window)
	{
		throw WindowException("SDL_CreateWindow() failed");
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

	m_context = SDL_GL_CreateContext(m_window);

	if(!m_context)
	{
		throw WindowException("SDL_GL_CreateContext() failed");
	}

	m_window_size = std::make_pair(width, height);
	m_window_center = std::make_pair(width / 2, height / 2);
	center_mouse();
}

Window::~Window()
{
	SDL_GL_DeleteContext(m_context);
	SDL_DestroyWindow(m_window);
	SDL_Quit();
}

void Window::update(bool mouse_input)
{
	SDL_Event event;

	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
				m_input.request_quit();
				break;

			case SDL_KEYDOWN:
				m_input.handle_key_down(static_cast<InputManager::Key>(event.key.keysym.scancode));
				break;

			case SDL_KEYUP:
				m_input.handle_key_up(static_cast<InputManager::Key>(event.key.keysym.scancode));
				break;

			case SDL_MOUSEBUTTONDOWN:
				if (mouse_input)
				{
					m_input.handle_mouse_down(static_cast<InputManager::MouseButton>(event.button.button));
				}
				
				break;

			case SDL_MOUSEBUTTONUP:
				if (mouse_input)
				{
					m_input.handle_mouse_up(static_cast<InputManager::MouseButton>(event.button.button));
				}

				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						m_window_size = std::make_pair(event.window.data1, event.window.data2);
						m_window_center = std::make_pair(event.window.data1 / 2, event.window.data2 / 2);

						if (mouse_input)
						{
							center_mouse();
						}
						
						for (auto& handler : m_window_resize_handlers)
						{
							handler(m_window_size);
						}

						break;
				}

				break;
		}
	}

	if (mouse_input)
	{
		int x;
		int y;

		SDL_GetMouseState(&x, &y);

		m_input.set_mouse_offset(std::make_pair(x - m_window_center.first, y - m_window_center.second));
		center_mouse();
	}
}

void Window::swap_buffers()
{
	SDL_GL_SwapWindow(m_window);
}