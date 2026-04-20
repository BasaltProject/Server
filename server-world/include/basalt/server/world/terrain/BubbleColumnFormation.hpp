#pragma once

#include <basalt/server/world/Chunk.hpp>

namespace basalt::server::world::terrain {

/// parity: water over soul sand or magma schedules a tick running `BubbleColumnBlock.updateColumn`.
namespace bubbleColumnFormation {

void formInto(Chunk& chunk);

} // namespace bubbleColumnFormation

} // namespace basalt::server::world::terrain
