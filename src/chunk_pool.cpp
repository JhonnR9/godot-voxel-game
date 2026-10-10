
#include "chunk_pool.h"
#include "chunk_node.h"
#include "godot_cpp/classes/node.hpp"

namespace godot {

void ChunkPool::set_owner(Node *owner) {
	this->_owner_node = owner;
}

void ChunkPool::_bind_methods() {
}

ChunkNode *ChunkPool::_allocate_one(const bool p_enable) {
	if (!_owner_node) return nullptr;
	auto *chunk = memnew(ChunkNode);
	_owner_node->add_child(chunk);
	if (p_enable) {
		chunk->enable();
	} else {
		chunk->disable();
		_pool.push_back(chunk);
	}
	++_allocated_count;
	return chunk;
}

void ChunkPool::_trim_idle_nodes() {
	while (_allocated_count > _target_capacity && !_pool.empty()) {
		ChunkNode *chunk = _pool.back();
		_pool.pop_back();
		if (chunk) {
			if (chunk->get_parent()) chunk->get_parent()->remove_child(chunk);
			chunk->queue_free();
		}
		--_allocated_count;
	}
}

ChunkNode *ChunkPool::acquire() {
	if (!_owner_node) return nullptr;

	if (_pool.empty()) {
		// Demand can temporarily exceed the prewarmed capacity while chunks
		// are being replaced. Grow transparently rather than failing a mesh.
		return _allocate_one(true);
	}
	ChunkNode *chunk = _pool.back();
	_pool.pop_back();
	chunk->enable();
	return chunk;
}

void ChunkPool::release(ChunkNode *chunk) {
	if (!chunk) return;

	chunk->disable();
	if (_allocated_count > _target_capacity) {
		if (chunk->get_parent()) chunk->get_parent()->remove_child(chunk);
		chunk->queue_free();
		--_allocated_count;
		return;
	}
	_pool.push_back(chunk);
}

void ChunkPool::set_prewarm(const int count) {
	_target_capacity = MAX(0, count);
	if (!_owner_node) return;
	_trim_idle_nodes();
	while (_allocated_count < _target_capacity) _allocate_one(false);
}

void ChunkPool::clear() {
	const int freed_count = static_cast<int>(_pool.size());
	for (ChunkNode *chunk : _pool) {
		if (chunk) {
			if (chunk->get_parent()) {
				chunk->get_parent()->remove_child(chunk);
			}
			chunk->queue_free();
		}
	}

	_pool.clear();
	_allocated_count = MAX(0, _allocated_count - freed_count);
}

} //namespace godot
