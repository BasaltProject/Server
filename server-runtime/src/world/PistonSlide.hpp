#pragma once

#include "PistonsShared.hpp"

#include <cstdint>
#include <vector>

/// The half of the piston that runs while blocks are in the air: the `moving_piston` squares, what
/// they carry and where they land. `Pistons.cpp` books the move and this ends it.
namespace basalt::server::runtime::world::pistons {

/// One block mid-slide: where its `moving_piston` stands, what it will become, and which way it is
/// going, because a pull back moves the other way from the piston's own face.
struct Carried {
    Position at;
    std::int32_t stateId = 0;
    bool extending = true;
};

/// The blocks mid-slide, furthest along first so nothing lands on a block that has not moved yet.
[[nodiscard]] std::vector<Carried> carriedBlocks(const Context& context, std::int32_t pistonX, std::int32_t pistonY,
                                                 std::int32_t pistonZ, registry::BlockFace direction);

/// parity: `PistonBaseBlock.triggerEvent`'s own check for a head that is still on its way out.
[[nodiscard]] bool isRetractingOntoSlide(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                         registry::BlockFace direction);

/// The push starting: every square that moves turns into a `moving_piston` carrying what it will be.
void beginMove(const Context& context, const Push& push, registry::BlockFace direction, bool sticky,
               std::int32_t pistonX, std::int32_t pistonY, std::int32_t pistonZ, const std::string& path,
               std::vector<BlockChange>& effects);

/// The pull back starting, which moves the body as well as whatever the head brings with it.
void beginRetract(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  registry::BlockFace direction, bool sticky, const std::string& path, bool headStillSliding);

} // namespace basalt::server::runtime::world::pistons
