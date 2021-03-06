#ifndef CUBED_WORLD_H
#define CUBED_WORLD_H

#include "block_type.h"
#include "block_info.h"
#include "chunk_update.h"
#include <array>
#include <atomic>
#include <functional>
#include <glm/include/glm.hpp>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

class Chunk;

class World
{
public:
	World(int render_distance);
	~World();

	void update(const glm::vec3& center);
	void render();

	void set_render_distance(int render_distance) { m_render_distance = render_distance; }

	BlockType get_block_type(int block_x, int block_y, int block_z) const;
	auto& get_block_properties(BlockType type) { return m_block_info.get_properties(type); }
	bool is_block_at(int block_x, int block_y, int block_z) const { return get_block_type(block_x, block_y, block_z) != BLOCK_AIR; }

private:
	typedef std::array<std::pair<std::unique_ptr<ChunkUpdate>, std::mutex>, 10> ChunkUpdateArray;

	void chunk_update_thread();
	void update_loaded_chunks(const glm::vec3& center);
	void load_chunk(int chunk_x, int chunk_y, int chunk_z);
	Chunk* get_block_chunk(int block_x, int block_y, int block_z) const;
	Chunk* get_chunk(int chunk_x, int chunk_y, int chunk_z) const;
	void for_each_chunk(std::function<bool(Chunk*, int, int, int)> callback, bool skip_not_filled = true);
	ChunkUpdateArray::value_type* get_chunk_update_slot(bool low_priority);
	ChunkUpdateArray::value_type* get_next_chunk_update();
	void process_completed_chunk_update(decltype(ChunkUpdateArray::value_type::first)& chunk_update);

	int m_render_distance;
	std::unordered_map<int, std::unordered_map<int, std::unordered_map<int, std::unique_ptr<Chunk>>>> m_chunks;
	ChunkUpdateArray m_chunk_updates;
	ChunkUpdateArray m_chunk_updates_low_priority;
	std::atomic_bool m_run_chunk_updates;
	std::thread m_chunk_update_thread;
	const BlockInfo m_block_info;

	const int MAX_CHUNK_MESH_UPDATES_PER_FRAME = 2;
};

#endif