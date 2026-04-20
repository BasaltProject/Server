#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `CarvedPumpkinBlock.trySpawnGolem`.
namespace golemPatterns {

enum class Golem : std::uint8_t { Snow, Iron, Copper };

struct Match {
    Golem kind = Golem::Snow;
    /// Every square of the rectangle, empty iron-cross corners included: vanilla clears the whole width by height.
    std::vector<std::array<std::int32_t, 3>> cells;
    std::array<std::int32_t, 3> spawn{};
    std::array<std::int32_t, 3> body{};
    std::int32_t bodyState = 0;
    std::int32_t pumpkinState = 0;
};

using IsCopper = std::function<bool(std::int32_t stateId)>;

/// parity: `trySpawnGolem`.
[[nodiscard]] std::optional<Match> find(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                        const IsCopper& isCopper = {});

/// parity: `CarvedPumpkinBlock.canSpawnGolem` matches the `*Base` patterns before the pumpkin is placed (dispenser).
[[nodiscard]] bool canComplete(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const IsCopper& isCopper = {});

[[nodiscard]] std::string_view entityTypeOf(Golem kind);

} // namespace golemPatterns

} // namespace basalt::server::runtime::world
