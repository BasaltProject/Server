#pragma once

#include <basalt/server/runtime/world/BlockPatterns.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `WitherSkullBlock.getOrCreateWitherFull`: `^^^`, `###`, `~#~`.
namespace witherPatterns {

struct Match {
    std::vector<blockPatterns::Cell> cells;
    /// parity: `getBlock(1, 2, 0)`, the foot of the T, which the wither snaps to 0.55 above.
    blockPatterns::Cell spawn{};
    bool alongX = false;
};

using IsBase = std::function<bool(std::int32_t stateId)>;

[[nodiscard]] std::optional<Match> find(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                        const IsBase& isBase);

[[nodiscard]] bool isWitherSkull(std::int32_t stateId);

/// parity: `getOrCreateWitherBase`.
[[nodiscard]] bool canComplete(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const IsBase& isBase);

} // namespace witherPatterns

} // namespace basalt::server::runtime::world
