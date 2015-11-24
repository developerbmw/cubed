#ifndef CUBED_WORLD_H
#define CUBED_WORLD_H

#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <glm/include/glm.hpp>
#include "block_type.h"
#include "chunk_update.h"

class Chunk;

class World
{
public:
	World(int render_distance);

	void update(const glm::vec3& center);
	void render();

	BlockType get_block_type(int block_x, int block_y, int block_z);

	static const Blocks blocks;

private:
	void update_loaded_chunks(const glm::vec3& center);
	void load_chunk(int chunk_x, int chunk_y, int chunk_z);
	Chunk* get_block_chunk(int block_x, int block_y, int block_z);
	Chunk* get_chunk(int chunk_x, int chunk_y, int chunk_z);
	void for_each_chunk(std::function<bool(Chunk*, int, int, int)> callback, bool skip_not_filled = true);

	int m_render_distance;
	std::unordered_map<int, std::unordered_map<int, std::unordered_map<int, std::unique_ptr<Chunk>>>> m_chunks;
	std::vector<std::unique_ptr<ChunkUpdate>> m_chunk_updates;
};

#endif