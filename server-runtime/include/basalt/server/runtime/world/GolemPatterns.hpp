#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The three figures a carved pumpkin finishes, and what finishing one does to the blocks.
///
/// parity: `CarvedPumpkinBlock.trySpawnGolem`, which runs from `onPlace`: the pumpkin looks for a
/// body under it, clears every square of the pattern and puts the golem where the pattern says.
namespace golemPatterns {

enum class Golem : std::uint8_t { Snow, Iron };

/// Where a match was found, in the order `clearPatternBlocks` walks it.
struct Match {
    Golem kind = Golem::Snow;
    /// Every square of the pattern's rectangle, the empty corners of the iron cross included:
    /// vanilla clears the whole width by height, not only the squares it asked for.
    std::vector<std::array<std::int32_t, 3>> cells;
    /// The square the golem snaps to, half a block in and a twentieth up.
    std::array<std::int32_t, 3> spawn{};
};

/// What the pumpkin at `x`, `y`, `z` completes, if it completes anything.
///
/// parity: the patterns are tried in `trySpawnGolem`'s own order, snow then iron, and each
/// `BlockPattern.find` walks a cube of its own size out of the pumpkin towards positive x, y and z in
/// all twenty four orientations. The first one that matches wins. The copper golem, which the jar
/// tries after these two, waits for the copper chest its body turns into.
[[nodiscard]] std::optional<Match> find(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The entity each figure is made of.
[[nodiscard]] std::string_view entityTypeOf(Golem kind);

} // namespace golemPatterns

} // namespace basalt::server::runtime::world
