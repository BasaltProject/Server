#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace basalt::server::world::region {

/// Why a chunk could not be read: an unknown block, a missing palette, a corrupt section.
struct ChunkNbtError {
    std::string message;
};

/// parity: `SerializableChunkData`, the on-disk chunk NBT. Sections keep the palette and optional data
/// longs, with bits per entry recomputed from the palette size. Light and heightmaps are derived, so
/// they are not read back.
namespace chunkNbt {

/// The world version this build writes. parity: the `DataVersion` of 26.3.
inline constexpr std::int32_t kDataVersion = 5023;

/// Where the entities standing in this chunk are kept. Vanilla uses a separate `entities` region
/// folder; here they ride with the chunk, one file and one lock.
inline constexpr std::string_view kEntitiesKey = "basalt_entities";
inline constexpr std::string_view kGeneratedEntitiesKey = "basalt_generated_entities";

[[nodiscard]] wire::nbt::NbtCompound write(const Chunk& chunk);

[[nodiscard]] std::expected<Chunk, ChunkNbtError> read(const wire::nbt::NbtCompound& tag,
                                                       ChunkHeight bounds = kOverworldHeight);

} // namespace chunkNbt

} // namespace basalt::server::world::region
