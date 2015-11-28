#include "chunk.h"
#include "world.h"
#include "world_constants.h"
#include "world_gen/world_gen.h"
#include <utility>

World::World() :
	m_render_distance{1},
	m_run_chunk_updates{true}
{
	ChunkUpdate::set_world(this);
	update_loaded_chunks(WorldGen::get_spawn_pos());

	// pre-generate world
	for_each_chunk([](Chunk* chunk, int x, int y, int z)
	{
		WorldGen::fill_chunk(*chunk->get_block_data(), x * WorldConstants::CHUNK_SIZE, y * WorldConstants::CHUNK_SIZE, z * WorldConstants::CHUNK_SIZE);
		chunk->set_filled(true);
		return true;
	}, false);

	m_chunk_update_thread = std::thread{std::bind(&World::chunk_update_thread, this)};
}

World::~World()
{
	m_run_chunk_updates = false;

	if (m_chunk_update_thread.joinable())
	{
		m_chunk_update_thread.join();
	}
}

void World::update(const glm::vec3& center)
{
	update_loaded_chunks(center);

	// We need to lock this when writing to elements of m_chunk_updates
	std::unique_lock<decltype(m_chunk_updates_mutex)> chunk_updates_lock(m_chunk_updates_mutex, std::defer_lock);

	for_each_chunk([this, &chunk_updates_lock](Chunk* chunk, int x, int y, int z)
	{
		if (!chunk->update_queued() && (!chunk->filled() || !chunk->up_to_date()))
		{
			auto chunk_update_slot = get_chunk_update_slot(chunk->low_priority_update());

			if (!chunk_update_slot)
			{
				// No free slots. End the for_each_chunk loop.
				return false;
			}

			auto chunk_update = std::make_unique<ChunkUpdate>(chunk->get_block_data(), x, y, z, !chunk->filled());

			{
				chunk_updates_lock.lock();
				*chunk_update_slot = std::move(chunk_update);
				chunk_updates_lock.unlock();
			}

			chunk->set_update_queued(true);
		}

		return true;
	}, false);

	// Limit to prevent stuttering
	int mesh_updates = MAX_CHUNK_MESH_UPDATES_PER_FRAME;

	for (auto& chunk_update : m_chunk_updates)
	{
		if (chunk_update && chunk_update->finished())
		{
			process_completed_chunk_update(chunk_update);

			chunk_updates_lock.lock();
			chunk_update.reset();
			chunk_updates_lock.unlock();
			
			if (--mesh_updates <= 0)
			{
				break;
			}
		}
	}

	if (mesh_updates > 0)
	{
		for (auto& chunk_update : m_chunk_updates_low_priority)
		{
			if (chunk_update && chunk_update->finished())
			{
				process_completed_chunk_update(chunk_update);

				chunk_updates_lock.lock();
				chunk_update.reset();
				chunk_updates_lock.unlock();

				if (--mesh_updates <= 0)
				{
					break;
				}
			}
		}
	}
}

void World::render()
{
	for_each_chunk([](Chunk* chunk, int x, int y, int z)
	{
		chunk->render();
		return true;
	});
}

BlockType World::get_block_type(int block_x, int block_y, int block_z)
{
	auto chunk = get_block_chunk(block_x, block_y, block_z);

	if (!chunk)
	{
		return BLOCK_AIR;
	}

	return chunk->get_block_type(static_cast<int>(block_x - chunk->get_x() * WorldConstants::CHUNK_SIZE), static_cast<int>(block_y - chunk->get_y() * WorldConstants::CHUNK_SIZE), static_cast<int>(block_z - chunk->get_z() * WorldConstants::CHUNK_SIZE));
}

void World::chunk_update_thread()
{
	while (m_run_chunk_updates)
	{
		auto chunk_update = get_next_chunk_update();

		if (!chunk_update)
		{
			// Avoid spinning waiting for a chunk update
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		(*chunk_update)->run();
		(*chunk_update)->set_finished();
	}
}

void World::update_loaded_chunks(const glm::vec3& center)
{
	int x = static_cast<int>((center.x < 0.0f) ? center.x - WorldConstants::CHUNK_SIZE - 1.0f : center.x);
	int y = static_cast<int>((center.y < 0.0f) ? center.y - WorldConstants::CHUNK_SIZE - 1.0f : center.y);
	int z = static_cast<int>((center.z < 0.0f) ? center.z - WorldConstants::CHUNK_SIZE - 1.0f : center.z);

	x /= WorldConstants::CHUNK_SIZE;
	y /= WorldConstants::CHUNK_SIZE;
	z /= WorldConstants::CHUNK_SIZE;

	int first_x = x - m_render_distance;
	int first_y = y - m_render_distance;
	int first_z = z - m_render_distance;

	int last_x = x + m_render_distance;
	int last_y = y + m_render_distance;
	int last_z = z + m_render_distance;

	for (x = first_x; x <= last_x; ++x)
	{
		for (y = first_y; y <= last_y; ++y)
		{
			for (z = first_z; z <= last_z; ++z)
			{
				if (!get_chunk(x, y, z))
					load_chunk(x, y, z);
			}
		}
	}

	bool del_x = false;
	bool del_y = false;

	for (auto it_x = m_chunks.begin(); it_x != m_chunks.end();)
	{
		for (auto it_y = it_x->second.begin(); it_y != it_x->second.end();)
		{
			for (auto it_z = it_y->second.begin(); it_z != it_y->second.end();)
			{
				if ((it_z->second->get_x() < first_x) || (it_z->second->get_x() > last_x))
				{
					del_x = true;

					break;
				}

				if ((it_z->second->get_y() < first_y) || (it_z->second->get_y() > last_y))
				{
					del_y = true;

					break;
				}

				if ((it_z->second->get_z() < first_z) || (it_z->second->get_z() > last_z))
				{
					it_z = it_y->second.erase(it_z);
				}
				else
				{
					++it_z;
				}
			}

			if (del_x)
			{
				break;
			}
			else if (del_y)
			{
				it_y = it_x->second.erase(it_y);
				del_y = false;
			}
			else
			{
				++it_y;
			}
		}

		if (del_x)
		{
			it_x = m_chunks.erase(it_x);
			del_x = false;
		}
		else
		{
			++it_x;
		}
	}
}

void World::load_chunk(int chunk_x, int chunk_y, int chunk_z)
{
	auto p1 = m_chunks.emplace(chunk_x, std::unordered_map<int, std::unordered_map<int, std::unique_ptr<Chunk>>>{});
	auto p2 = p1.first->second.emplace(chunk_y, std::unordered_map<int, std::unique_ptr<Chunk>>{});
	p2.first->second.emplace(chunk_z, std::make_unique<Chunk>(chunk_x, chunk_y, chunk_z));
}

Chunk* World::get_block_chunk(int block_x, int block_y, int block_z)
{
	int chunk_x = (block_x < 0 ? block_x - WorldConstants::CHUNK_SIZE + 1 : block_x) / WorldConstants::CHUNK_SIZE;
	int chunk_y = (block_y < 0 ? block_y - WorldConstants::CHUNK_SIZE + 1 : block_y) / WorldConstants::CHUNK_SIZE;
	int chunk_z = (block_z < 0 ? block_z - WorldConstants::CHUNK_SIZE + 1 : block_z) / WorldConstants::CHUNK_SIZE;

	return get_chunk(chunk_x, chunk_y, chunk_z);
}

Chunk* World::get_chunk(int chunk_x, int chunk_y, int chunk_z)
{
	auto itx = m_chunks.find(chunk_x);

	if (itx == m_chunks.end())
	{
		return nullptr;
	}

	auto ity = itx->second.find(chunk_y);

	if (ity == itx->second.end())
	{
		return nullptr;
	}

	auto itz = ity->second.find(chunk_z);

	if (itz == ity->second.end())
	{
		return nullptr;
	}

	return itz->second.get();
}

void World::for_each_chunk(std::function<bool(Chunk*, int, int, int)> callback, bool skip_not_filled)
{
	for (auto& x : m_chunks)
	{
		for (auto& y : x.second)
		{
			for (auto& z : y.second)
			{
				if (z.second->filled())
				{
					if (!callback(z.second.get(), x.first, y.first, z.first))
					{
						return;
					}
				}
				else if (!skip_not_filled && !callback(z.second.get(), x.first, y.first, z.first))
				{
					return;
				}
			}
		}
	}
}

World::ChunkUpdateArray::value_type* World::get_chunk_update_slot(bool low_priority)
{
	// No need for read locking since only this thread writes to it.
	for (auto& chunk_update : (low_priority ? m_chunk_updates_low_priority : m_chunk_updates))
	{
		if (!chunk_update)
		{
			return &chunk_update;
		}
	}

	return nullptr;
}

World::ChunkUpdateArray::value_type* World::get_next_chunk_update()
{
	std::lock_guard<decltype(m_chunk_updates_mutex)> chunk_updates_lock(m_chunk_updates_mutex);

	for (auto& chunk_update : m_chunk_updates)
	{
		if (chunk_update && !chunk_update->finished())
		{
			return &chunk_update;
		}
	}

	for (auto& chunk_update : m_chunk_updates_low_priority)
	{
		if (chunk_update && !chunk_update->finished())
		{
			return &chunk_update;
		}
	}

	return nullptr;
}

void World::process_completed_chunk_update(World::ChunkUpdateArray::value_type& chunk_update)
{
	auto chunk = get_chunk(chunk_update->get_x(), chunk_update->get_y(), chunk_update->get_z());

	if (!chunk)
	{
		// Chunk has been unloaded while we were updating it's data
		return;
	}

	chunk->update_mesh(chunk_update->get_vertices().data(), chunk_update->get_indices().data(), chunk_update->get_num_vertices(), chunk_update->get_num_indices());

	chunk->set_filled(true);
	chunk->set_up_to_date(true);
	chunk->set_update_queued(false);
	chunk->set_low_priority_update(false);
}