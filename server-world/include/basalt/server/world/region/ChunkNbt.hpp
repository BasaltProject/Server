#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace basalt::server::world::region {

struct ChunkNbtError {
    std::string message;
};

/// parity: `SerializableChunkData`.
namespace chunkNbt {

/// The world version this build writes. parity: the `DataVersion` of 26.3.
inline constexpr std::int32_t kDataVersion = 5023;

inline constexpr std::string_view kEntitiesKey = "basalt_entities";
inline constexpr std::string_view kGeneratedEntitiesKey = "basalt_generated_entities";

[[nodiscard]] wire::nbt::NbtCompound write(const Chunk& chunk);

[[nodiscard]] std::expected<Chunk, ChunkNbtError> read(const wire::nbt::NbtCompound& tag,
                                                       ChunkHeight bounds = kOverworldHeight);

} // namespace chunkNbt

} // namespace basalt::server::world::region
