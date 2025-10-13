#pragma once

#include <basalt/server/world/Chunk.hpp>

namespace basalt::server::world::terrain {

/// parity: water over soul sand or magma schedules a tick running `BubbleColumnBlock.updateColumn`,
/// so a generated chunk is saved with its columns. Nothing ticks yet, so it is built after
/// decoration.
namespace bubbleColumnFormation {

void formInto(Chunk& chunk);

} // namespace bubbleColumnFormation

} // namespace basalt::server::world::terrain
