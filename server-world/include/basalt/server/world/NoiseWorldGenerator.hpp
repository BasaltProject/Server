#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world {

namespace noiseWorldGenerator {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ);

} // namespace noiseWorldGenerator

} // namespace basalt::server::world
