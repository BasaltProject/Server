#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world {

/// parity: `TreeFeature.updateLeaves`.
namespace leafDistances {

std::int32_t assign(Chunk& chunk);

} // namespace leafDistances

} // namespace basalt::server::world
