#include "game.h"
#include "world_gen/world_gen.h"
#include <chrono>
#include <thread>

Game::Game() :
	m_window{"Cubed", 800, 600, m_input_manager},
	m_rendering_engine(m_window),
	m_player{m_input_manager, WorldGen::get_spawn_pos()}
{
	#ifdef _DEBUG
		m_world.set_render_distance(2);
	#else
		m_world.set_render_distance(5);
	#endif

	m_rendering_engine.load_shader("basic_shader", {"position", "texCoord"}, {{UNIFORMTYPE_MAT4, "transform"}});
	m_rendering_engine.use_shader("basic_shader");
	
	m_rendering_engine.load_texture("blocks.png");
	m_rendering_engine.use_texture("blocks.png");
}

void Game::run()
{
	#ifdef _DEBUG
		const auto TARGET_FPS = 30;
	#else
		const auto TARGET_FPS = 60;
	#endif

	const auto FRAME_DURATION = std::chrono::nanoseconds{std::nano::den / TARGET_FPS};
	std::chrono::nanoseconds unprocessed_time{0};
	auto last_update_time = std::chrono::steady_clock::now() - FRAME_DURATION;

	while (!m_input_manager.is_quit_requested())
	{
		auto now = std::chrono::steady_clock::now();
		unprocessed_time += now - last_update_time;
		last_update_time = now;
		bool needs_render = false;

		while (unprocessed_time >= FRAME_DURATION)
		{
			unprocessed_time -= FRAME_DURATION;
			needs_render = true;

			update();
		}

		if (needs_render)
		{
			render();
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void Game::update()
{
	m_window.update();
	m_player.update();
	m_world.update(m_player.get_camera().get_position());
	m_rendering_engine.set_mat4("transform", m_rendering_engine.get_projection_matrix() * m_player.get_camera().get_matrix());
	m_rendering_engine.update_uniforms();
}

void Game::render()
{
	m_rendering_engine.clear();
	m_world.render();
	m_window.swap_buffers();
}