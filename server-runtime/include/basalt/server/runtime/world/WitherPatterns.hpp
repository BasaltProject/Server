#pragma once

#include <basalt/server/runtime/world/BlockPatterns.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// The T of soul sand and the three skulls that summon a wither.
///
/// parity: `WitherSkullBlock.getOrCreateWitherFull`: `^^^`, `###`, `~#~`.
namespace witherPatterns {

struct Match {
    /// Every square of the three by three, in the order `clearPatternBlocks` walks it.
    std::vector<blockPatterns::Cell> cells;
    /// parity: `getBlock(1, 2, 0)`, the foot of the T, which the wither snaps to 0.55 above.
    blockPatterns::Cell spawn{};
    /// Whether the match's forwards, across the T, lies along x: yaw 0 then, 90 otherwise.
    bool alongX = false;
};

/// Whether a state is in `minecraft:wither_summon_base_blocks`.
using IsBase = std::function<bool(std::int32_t stateId)>;

[[nodiscard]] std::optional<Match> find(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                        const IsBase& isBase);

[[nodiscard]] bool isWitherSkull(std::int32_t stateId);

} // namespace witherPatterns

} // namespace basalt::server::runtime::world
