#include "world.h"
#include "chunk.h"
#include "world_gen/world_gen.h"
#include "world_constants.h"

const Blocks World::blocks;

World::World(int render_distance) :
	m_render_distance{render_distance < 1 ? 1 : render_distance}
{
	update_loaded_chunks(WorldGen::get_spawn_pos());
}

void World::update(const glm::vec3& center)
{
	update_loaded_chunks(center);

	for_each_chunk([this](Chunk* chunk, int x, int y, int z)
	{
		if (!chunk->update_queued() && (!chunk->filled() || !chunk->up_to_date()))
		{
			m_chunk_updates.emplace_back(new ChunkUpdate(chunk->get_blocks(), x, y, z, !chunk->filled()));
			chunk->set_update_queued(true);
		}

		return true;
	}, false);

	// To go in separate thread with locks on the vector
	int limit = 3;
	for (auto& cu : m_chunk_updates)
	{
		cu->run();

		if (--limit <= 0)
		{
			break;
		}
	}

	for (auto it = m_chunk_updates.begin(); it != m_chunk_updates.end();)
	{
		auto cu = it->get();

		if (cu->finished())
		{
			auto chunk = get_chunk(cu->get_x(), cu->get_y(), cu->get_z());

			if (chunk)
			{
				chunk->update_mesh(cu->get_vertices(), cu->get_indices(), cu->get_num_vertices(), cu->get_num_indices());
				chunk->set_filled(true);
				chunk->set_up_to_date(true);
				chunk->set_update_queued(false);
			}

			it = m_chunk_updates.erase(it);
		}
		else
		{
			++it;
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
	auto p3 = p2.first->second.emplace(chunk_z, std::make_unique<Chunk>(chunk_x, chunk_y, chunk_z));

	Chunk* chunk;

	chunk = get_chunk(chunk_x - 1, chunk_y, chunk_z);
	if (chunk)
	{
		chunk->set_up_to_date(false);
	}

	chunk = get_chunk(chunk_x + 1, chunk_y, chunk_z);
	if (chunk)
	{
		chunk->set_up_to_date(false);
	}

	chunk = get_chunk(chunk_x, chunk_y - 1, chunk_z);
	if (chunk)
	{
		chunk->set_up_to_date(false);
	}

	chunk = get_chunk(chunk_x, chunk_y + 1, chunk_z);
	if (chunk)
	{
		chunk->set_up_to_date(false);
	}

	chunk = get_chunk(chunk_x, chunk_y, chunk_z - 1);
	if (chunk)
	{
		chunk->set_up_to_date(false);
	}

	chunk = get_chunk(chunk_x, chunk_y, chunk_z + 1);
	if (chunk)
	{
		chunk->set_up_to_date(false);
	}
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