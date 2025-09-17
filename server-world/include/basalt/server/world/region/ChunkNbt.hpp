#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <expected>
#include <string>

namespace basalt::server::world::region {

/// Why a chunk could not be read: an unknown block, a missing palette, a corrupt section.
struct ChunkNbtError {
    std::string message;
};

/// parity: `SerializableChunkData`, the on-disk chunk NBT. Sections carry the wire format's
/// palette-plus-optional-data-longs shape with no explicit bits-per-entry: both sides recompute it
/// from the palette size.
///
/// Light data and heightmaps stay out of scope: both are derived, and reading them back would be
/// trusting a file over the blocks it came with.
namespace chunkNbt {

/// The world version this build writes. parity: the `DataVersion` of 26.2.
inline constexpr std::int32_t kDataVersion = 4903;

/// Where the entities standing in this chunk are kept.
///
/// Vanilla moved them to a region folder of their own in 1.17; here they ride with the chunk, which
/// is one file to read and one lock to take, and nothing outside this server reads it.
inline constexpr std::string_view kEntitiesKey = "basalt_entities";
inline constexpr std::string_view kGeneratedEntitiesKey = "basalt_generated_entities";

[[nodiscard]] wire::nbt::NbtCompound write(const Chunk& chunk);

[[nodiscard]] std::expected<Chunk, ChunkNbtError> read(const wire::nbt::NbtCompound& tag,
                                                       ChunkHeight bounds = kOverworldHeight);

} // namespace chunkNbt

} // namespace basalt::server::world::region
