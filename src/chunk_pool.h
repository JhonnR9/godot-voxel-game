#ifndef CHUNK_POOL_H
#define CHUNK_POOL_H
#include "godot_cpp/classes/ref.hpp"

#include <mutex>
#include <vector>

namespace godot {

class Node;

class ChunkNode;

class ChunkPool final : public RefCounted{
	GDCLASS(ChunkPool, RefCounted)

protected:

	static void _bind_methods();
public:
	ChunkNode *acquire();
	void release(ChunkNode *chunk);
	void set_prewarm(int count);
	void set_owner(Node *owner);
	void clear();

private:
	Node *_owner_node = nullptr;
	std::vector<ChunkNode *> _pool;
	int _allocated_count = 0;
	int _target_capacity = 0;

	ChunkNode *_allocate_one(bool p_enable);
	void _trim_idle_nodes();
};

} //namespace godot

#endif //CHUNK_POOL_H
