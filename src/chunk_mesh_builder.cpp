#include "chunk_mesh_builder.h"
#include "voxel_mesher.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <algorithm>

namespace godot {
bool ChunkMeshBuilder::_is_air(const ChunkNeighbors &n, int x, int y, int z) {
	return voxel::is_air(_get_block(n, x, y, z));
}

voxel::Block ChunkMeshBuilder::_get_block(const ChunkNeighbors &n, int x, int y, int z) {
	if (x >= Chunk::MIN_X && x <= Chunk::MAX_X &&
		y >= Chunk::MIN_Y && y <= Chunk::MAX_Y &&
		z >= Chunk::MIN_Z && z <= Chunk::MAX_Z) {
		return n.center->get_block(x, y, z);
	}

	const int dx = x < 0 ? -1 : (x >= Chunk::SIZE_X ? 1 : 0);
	const int dy = y < 0 ? -1 : (y >= Chunk::SIZE_Y ? 1 : 0);
	const int dz = z < 0 ? -1 : (z >= Chunk::SIZE_Z ? 1 : 0);
	const auto &chunk = n.halo[ChunkNeighbors::halo_index(dx, dy, dz)];
	return chunk ? chunk->get_block(x - dx * Chunk::SIZE_X,
			y - dy * Chunk::SIZE_Y, z - dz * Chunk::SIZE_Z) : voxel::Block{0};
}

bool ChunkMeshBuilder::_is_face_visible(const ChunkNeighbors &n, int x, int y, int z, const voxel::Block current_block) {
	const voxel::Block neighbor = _get_block(n, x, y, z);
	return voxel::is_air(neighbor) ||
			(voxel::is_collidable(current_block) &&
					(voxel::is_transparent(neighbor) || voxel::is_cutout(neighbor)));
}

bool ChunkMeshBuilder::_is_crossed_plant(const voxel::Block block) {
	return voxel::has_flag(block, voxel::BLOCK_FLAG_CROSSED) ||
			voxel::has_flag(voxel::default_block_flags(voxel::block_id(block)), voxel::BLOCK_FLAG_CROSSED);
}

VoxelMesher &ChunkMeshBuilder::_get_mesher(const voxel::Block block) {
	return voxel::is_transparent(block) ? transparent_mesher : opaque_mesher;
}

void ChunkMeshBuilder::_add_faces(const ChunkNeighbors &neighbors, const CubeFace face) {
	const Chunk *center = neighbors.center.get();
	const bool x_face = face == CubeFace::R || face == CubeFace::L;
	const bool y_face = face == CubeFace::U || face == CubeFace::D;
	const int depth_count = x_face ? Chunk::SIZE_X : (y_face ? Chunk::SIZE_Y : Chunk::SIZE_Z);
	const int width = y_face ? Chunk::SIZE_Z : (x_face ? Chunk::SIZE_Z : Chunk::SIZE_X);
	const int height = x_face ? Chunk::SIZE_Y : (y_face ? Chunk::SIZE_X : Chunk::SIZE_Y);
	mask.resize(static_cast<size_t>(width * height));
	ao.resize(static_cast<size_t>(width * height));
	visited.resize(static_cast<size_t>(width * height));

	auto index = [width](const int u, const int v) { return static_cast<size_t>(v * width + u); };
	auto block_at = [center, face](const int depth, const int u, const int v) {
		if (face == CubeFace::R || face == CubeFace::L) return center->get_block(depth, v, u);
		if (face == CubeFace::U || face == CubeFace::D) return center->get_block(v, depth, u);
		return center->get_block(u, v, depth);
	};
	auto neighbor_is_visible = [&](const int depth, const int u, const int v, const voxel::Block block) {
		if (face == CubeFace::R) return _is_face_visible(neighbors, depth + 1, v, u, block);
		if (face == CubeFace::L) return _is_face_visible(neighbors, depth - 1, v, u, block);
		if (face == CubeFace::U) return _is_face_visible(neighbors, v, depth + 1, u, block);
		if (face == CubeFace::D) return _is_face_visible(neighbors, v, depth - 1, u, block);
		if (face == CubeFace::F) return _is_face_visible(neighbors, u, v, depth + 1, block);
		return _is_face_visible(neighbors, u, v, depth - 1, block);
	};

	// AO samples the air-side layer of each face; transparent/cutout blocks
	// do not cast a solid cube's occlusion over their neighbours.
	const int outward = (face == CubeFace::R || face == CubeFace::U || face == CubeFace::F) ? 1 : -1;
	auto occludes = [&](int depth, int u, int v) {
		const auto block = x_face ? _get_block(neighbors, depth, v, u) :
				(y_face ? _get_block(neighbors, v, depth, u) : _get_block(neighbors, u, v, depth));
		return voxel::is_collidable(block) && !voxel::is_cutout(block);
	};
	auto face_ao = [&](int depth, int u, int v) {
		std::array<float, 4> result{};
		const int du[4] = {-1, 1, 1, -1};
		const int dv[4] = {-1, -1, 1, 1};
		for (int corner = 0; corner < 4; ++corner) {
			const bool side_u = occludes(depth + outward, u + du[corner], v);
			const bool side_v = occludes(depth + outward, u, v + dv[corner]);
			const bool diagonal = occludes(depth + outward, u + du[corner], v + dv[corner]);
			result[corner] = side_u && side_v ? 0.0f : 1.0f - float(side_u + side_v + diagonal) / 3.0f;
		}
		return result;
	};
	auto uniform_ao = [](const std::array<float, 4> &value) {
		return value[0] == value[1] && value[0] == value[2] && value[0] == value[3];
	};

	for (int depth = 0; depth < depth_count; ++depth) {
		std::fill(mask.begin(), mask.end(), 0);
		std::fill(visited.begin(), visited.end(), 0);
		for (int v = 0; v < height; ++v) {
			for (int u = 0; u < width; ++u) {
				const voxel::Block block = block_at(depth, u, v);
				if (voxel::is_air(block) || _is_crossed_plant(block) ||
						(face != CubeFace::U && voxel::type(block) == voxel::block_ids::water)) continue;
				mask[index(u, v)] = neighbor_is_visible(depth, u, v, block);
				if (mask[index(u, v)]) ao[index(u, v)] = face_ao(depth, u, v);
			}
		}

		for (int v = 0; v < height; ++v) {
			for (int u = 0; u < width; ++u) {
				const size_t cell = index(u, v);
				if (!mask[cell] || visited[cell]) continue;
				const voxel::Block block = block_at(depth, u, v);
				const uint16_t type = voxel::type(block);
				int quad_w = 1;
				int quad_h = 1;
				while (uniform_ao(ao[cell]) && u + quad_w < width) {
					const size_t next = index(u + quad_w, v);
					if (!mask[next] || visited[next] || voxel::type(block_at(depth, u + quad_w, v)) != type || ao[next] != ao[cell]) break;
					++quad_w;
				}
				bool can_expand = uniform_ao(ao[cell]);
				while (v + quad_h < height && can_expand) {
					for (int k = 0; k < quad_w; ++k) {
						const size_t next = index(u + k, v + quad_h);
						if (!mask[next] || visited[next] || voxel::type(block_at(depth, u + k, v + quad_h)) != type || ao[next] != ao[cell]) {
							can_expand = false;
						break;
						}
					}
					if (can_expand) ++quad_h;
				}
				for (int dv = 0; dv < quad_h; ++dv)
					for (int du = 0; du < quad_w; ++du)
						visited[index(u + du, v + dv)] = 1;

				Vector3 v0, v1, v2, v3, normal;
				Vector2 uv_scale;
				bool swap_uvs = false;
				if (face == CubeFace::R) {
					v0 = Vector3(depth + 1, v, u); v1 = Vector3(depth + 1, v + quad_h, u);
					v2 = Vector3(depth + 1, v + quad_h, u + quad_w); v3 = Vector3(depth + 1, v, u + quad_w);
					normal = Vector3(1, 0, 0); uv_scale = Vector2(quad_h, quad_w); swap_uvs = true;
				} else if (face == CubeFace::L) {
					v0 = Vector3(depth, v, u); v1 = Vector3(depth, v, u + quad_w);
					v2 = Vector3(depth, v + quad_h, u + quad_w); v3 = Vector3(depth, v + quad_h, u);
					normal = Vector3(-1, 0, 0); uv_scale = Vector2(quad_w, quad_h);
				} else if (face == CubeFace::U) {
					v0 = Vector3(v, depth + 1, u + quad_w); v1 = Vector3(v + quad_h, depth + 1, u + quad_w);
					v2 = Vector3(v + quad_h, depth + 1, u); v3 = Vector3(v, depth + 1, u);
					normal = Vector3(0, 1, 0); uv_scale = Vector2(quad_h, quad_w);
				} else if (face == CubeFace::D) {
					v0 = Vector3(v, depth, u); v1 = Vector3(v + quad_h, depth, u);
					v2 = Vector3(v + quad_h, depth, u + quad_w); v3 = Vector3(v, depth, u + quad_w);
					normal = Vector3(0, -1, 0); uv_scale = Vector2(quad_h, quad_w); swap_uvs = true;
				} else if (face == CubeFace::F) {
					v0 = Vector3(u, v, depth + 1); v1 = Vector3(u + quad_w, v, depth + 1);
					v2 = Vector3(u + quad_w, v + quad_h, depth + 1); v3 = Vector3(u, v + quad_h, depth + 1);
					normal = Vector3(0, 0, 1); uv_scale = Vector2(quad_w, quad_h);
				} else {
					v0 = Vector3(u + quad_w, v, depth); v1 = Vector3(u, v, depth);
					v2 = Vector3(u, v + quad_h, depth); v3 = Vector3(u + quad_w, v + quad_h, depth);
					normal = Vector3(0, 0, -1); uv_scale = Vector2(quad_w, quad_h);
				}
				const auto &corners = ao[cell];
				std::array<float, 4> vertex_ao = corners;
				if (face == CubeFace::R || face == CubeFace::D)
					vertex_ao = {corners[0], corners[3], corners[2], corners[1]};
				else if (face == CubeFace::U)
					vertex_ao = {corners[1], corners[2], corners[3], corners[0]};
				else if (face == CubeFace::B)
					vertex_ao = {corners[1], corners[0], corners[3], corners[2]};
				_get_mesher(block).add_quad(v0, v1, v2, v3, normal,
						_get_tex_layer(face, type), uv_scale, swap_uvs, voxel::is_collidable(block), _get_block_tint(type), vertex_ao);
			}
		}
	}
}

int ChunkMeshBuilder::_get_tex_layer(const CubeFace &face, const uint16_t type) {
    if (type == voxel::block_ids::water) return voxel::WATER_TEXTURE_LAYER;
    return metadata->layers[type][static_cast<int>(face)];
}
Color ChunkMeshBuilder::_get_block_tint(const uint16_t type) const { return metadata->tints[type]; }
std::shared_ptr<const ChunkMeshMetadata> ChunkMeshBuilder::load_metadata() {
    auto data = std::make_shared<ChunkMeshMetadata>();
	Ref<FileAccess> file = FileAccess::open("res://data/block_registry.generated.json", FileAccess::READ);
	if (file.is_null()) {
		ERR_PRINT("Could not load generated block registry metadata.");
		return data;
	}

	String json_text = file->get_as_text();
	Variant parsed     = JSON::parse_string(json_text);

	if (parsed.get_type() != Variant::DICTIONARY) {
		ERR_PRINT("Generated block registry metadata is invalid.");
		return data;
	}

	Dictionary root = parsed;
	Array blocks = root.get("blocks", Array());
	for (int i = 0; i < blocks.size(); ++i) {
		Dictionary block = blocks[i];
		const uint16_t id = static_cast<uint16_t>(int(block.get("id", 0)));
        if (id >= data->layers.size()) continue;
		Array tint = block.get("tint", Array());
		if (tint.size() == 4) {
			data->tints[id] = Color(double(tint[0]), double(tint[1]), double(tint[2]), double(tint[3]));
		}
		Dictionary faces = block.get("texture_layers", Dictionary());
		const int side = int(faces.get("side", -1));
		const int top = int(faces.get("top", side));
		const int bottom = int(faces.get("bottom", side));
		if (side >= 0) {
			data->layers[id][static_cast<int>(CubeFace::F)] = side;
			data->layers[id][static_cast<int>(CubeFace::B)] = side;
			data->layers[id][static_cast<int>(CubeFace::L)] = side;
			data->layers[id][static_cast<int>(CubeFace::R)] = side;
		}
		if (top >= 0) data->layers[id][static_cast<int>(CubeFace::U)] = top;
		if (bottom >= 0) data->layers[id][static_cast<int>(CubeFace::D)] = bottom;
	}
    return data;
}

void ChunkMeshBuilder::_add_crossed_plant_faces(const ChunkNeighbors &neighbors) {
	const Chunk *center = neighbors.center.get();
	for (int z = 0; z < Chunk::SIZE_Z; ++z) {
		for (int y = 0; y < Chunk::SIZE_Y; ++y) {
			for (int x = 0; x < Chunk::SIZE_X; ++x) {
				const voxel::Block block = center->get_block(x, y, z);
				if (!_is_crossed_plant(block)) continue;
				selection_positions.push_back(Vector3(x, y, z));
				if (voxel::has_flag(block, voxel::BLOCK_FLAG_EMISSIVE) ||
						voxel::has_flag(voxel::default_block_flags(voxel::block_id(block)), voxel::BLOCK_FLAG_EMISSIVE)) {
					light_positions.push_back(Vector3(x + 0.5f, y + 0.8f, z + 0.5f));
				}
				const int layer = _get_tex_layer(CubeFace::F, voxel::type(block));
				const Vector3 a(x + 0.12f, y, z + 0.12f);
				const Vector3 b(x + 0.88f, y, z + 0.88f);
				const Vector3 c(x + 0.88f, y + 1.0f, z + 0.88f);
				const Vector3 d(x + 0.12f, y + 1.0f, z + 0.12f);
				const Vector3 e(x + 0.12f, y, z + 0.88f);
				const Vector3 f(x + 0.88f, y, z + 0.12f);
				const Vector3 g(x + 0.88f, y + 1.0f, z + 0.12f);
				const Vector3 h(x + 0.12f, y + 1.0f, z + 0.88f);
				const Vector2 uv_scale(1.0f, 1.0f);
				const Vector3 normal_a(0.707f, 0.0f, -0.707f);
				const Vector3 normal_b(-0.707f, 0.0f, -0.707f);
				const Color tint = _get_block_tint(voxel::type(block));
				opaque_mesher.add_quad(a, b, c, d, normal_a, layer, uv_scale, false, false, tint);
				opaque_mesher.add_quad(a, d, c, b, -normal_a, layer, uv_scale, true, false, tint);
				opaque_mesher.add_quad(e, f, g, h, normal_b, layer, uv_scale, false, false, tint);
				opaque_mesher.add_quad(e, h, g, f, -normal_b, layer, uv_scale, true, false, tint);
			}
		}
	}
}

ChunkMeshBuilder::ChunkMeshBuilder(std::shared_ptr<const ChunkMeshMetadata> p_metadata) : metadata(std::move(p_metadata)) {}

ChunkMeshData ChunkMeshBuilder::build(const ChunkNeighbors &neighbors) {
	opaque_mesher.clear();
	light_positions.clear();
	selection_positions.clear();
	transparent_mesher.clear();
    if (neighbors.center->is_empty()) return {};

	_add_faces(neighbors, CubeFace::U);
	_add_faces(neighbors, CubeFace::L);
	_add_faces(neighbors, CubeFace::R);
	_add_faces(neighbors, CubeFace::D);
	_add_faces(neighbors, CubeFace::F);
	_add_faces(neighbors, CubeFace::B);
	_add_crossed_plant_faces(neighbors);

	Array opaque_arrays = opaque_mesher.build_arrays();
	Array transparent_arrays = transparent_mesher.build_arrays();
	const bool has_opaque = !PackedVector3Array(opaque_arrays[Mesh::ARRAY_VERTEX]).is_empty();
	const bool has_transparent = !PackedVector3Array(transparent_arrays[Mesh::ARRAY_VERTEX]).is_empty();
	if (!has_opaque && !has_transparent) {
		return {};
	}

    return {opaque_arrays, transparent_arrays, has_opaque, has_transparent};
}
} // namespace godot
