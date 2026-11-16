#pragma once

#include <basalt/server/world/region/ChunkNbt.hpp>

#include <span>

namespace basalt::server::world::region::chunkNbt {

[[nodiscard]] wire::nbt::NbtTag encodeTicks(const Chunk& chunk, std::span<const ChunkScheduledTick> ticks);

[[nodiscard]] wire::nbt::NbtTag encodeGeneratedEntity(const ChunkGeneratedEntity& entity);

[[nodiscard]] wire::nbt::NbtCompound encodeStructures(const structure::ChunkStructures& structures);

[[nodiscard]] std::expected<ChunkGeneratedEntity, ChunkNbtError>
decodeGeneratedEntity(const wire::nbt::NbtCompound& tag);

void decodeStructures(const wire::nbt::NbtCompound* tag, structure::ChunkStructures& into);

} // namespace basalt::server::world::region::chunkNbt
