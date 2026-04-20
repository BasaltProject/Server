#pragma once

#include "PistonsShared.hpp"

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world::pistons {

struct Carried {
    Position at;
    std::int32_t stateId = 0;
    bool extending = true;
};

/// The blocks mid-slide, furthest along first so nothing lands on a block that has not moved yet.
[[nodiscard]] std::vector<Carried> carriedBlocks(const Context& context, std::int32_t pistonX, std::int32_t pistonY,
                                                 std::int32_t pistonZ, registry::BlockFace direction);

/// parity: `PistonBaseBlock.checkIfExtend`'s check for a head still on its way out.
[[nodiscard]] bool isRetractingOntoSlide(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                         registry::BlockFace direction);

void beginMove(const Context& context, const Push& push, registry::BlockFace direction, bool sticky,
               std::int32_t pistonX, std::int32_t pistonY, std::int32_t pistonZ, const std::string& path,
               std::vector<BlockChange>& effects);

void beginRetract(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  registry::BlockFace direction, bool sticky, const std::string& path, bool headStillSliding);

} // namespace basalt::server::runtime::world::pistons
