#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The three figures a carved pumpkin finishes, and what finishing one does to the blocks.
///
/// parity: `CarvedPumpkinBlock.trySpawnGolem`, which runs from `onPlace`: the pumpkin looks for a
/// body under it, clears every square of the pattern and puts the golem where the pattern says.
namespace golemPatterns {

enum class Golem : std::uint8_t { Snow, Iron, Copper };

/// Where a match was found, in the order `clearPatternBlocks` walks it.
struct Match {
    Golem kind = Golem::Snow;
    /// Every square of the pattern's rectangle, the empty corners of the iron cross included:
    /// vanilla clears the whole width by height, not only the squares it asked for.
    std::vector<std::array<std::int32_t, 3>> cells;
    /// The square the golem snaps to, half a block in and a twentieth up.
    std::array<std::int32_t, 3> spawn{};
    /// The copper golem's body, which becomes a chest rather than air, and the state it had.
    std::array<std::int32_t, 3> body{};
    std::int32_t bodyState = 0;
    /// The pumpkin's own state, which is where the chest takes its facing from.
    std::int32_t pumpkinState = 0;
};

/// Whether a state is in `minecraft:copper`, which is the copper golem's body.
using IsCopper = std::function<bool(std::int32_t stateId)>;

/// What the pumpkin at `x`, `y`, `z` completes, if it completes anything. parity: the three patterns
/// are tried in `trySpawnGolem`'s order, snow then iron then copper, each walking a cube of its own
/// size in all twenty four orientations, first match wins. Without `isCopper` the copper golem can
/// never match.
[[nodiscard]] std::optional<Match> find(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                        const IsCopper& isCopper = {});

/// Whether a carved pumpkin put at these coordinates would finish a figure, asked **before** the
/// pumpkin is there. parity: `CarvedPumpkinBlock.canSpawnGolem`, which matches the three `*Base`
/// patterns, the ones without the pumpkin in them. It is what a dispenser asks before it places one.
[[nodiscard]] bool canComplete(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const IsCopper& isCopper = {});

/// The entity each figure is made of.
[[nodiscard]] std::string_view entityTypeOf(Golem kind);

} // namespace golemPatterns

} // namespace basalt::server::runtime::world
