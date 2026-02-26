#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::runtime::world {

/// The rail that notices a minecart standing on it.
///
/// parity: `DetectorRailBlock.checkPressed`: while something is on it, it is on and books twenty
/// ticks to look again; the tick it finds nothing, it goes out. It feeds every side weakly and the
/// block underneath strongly, which is what the redstone pass already reads off its own state.
namespace detectorRails {
/// parity: `EntityTypes.MINECART.sized(0.98F, 0.7F)`, half of it: how far a cart's box reaches to
/// each side, which is what decides how many rails it is touching.
inline constexpr double kCartHalfWidth = 0.49;

/// The other half of `EntityTypes.MINECART.sized(0.98F, 0.7F)`: how tall a cart's box is.
inline constexpr double kCartHeight = 0.7;

/// parity: the twenty ticks a pressed rail waits before checking again.
inline constexpr std::int32_t kRecheckTicks = 20;

/// parity: `AbstractMinecart`, of which any kind presses the rail.
[[nodiscard]] bool isMinecart(const wire::common::Identifier& type);

/// What one pass of `checkPressed` left behind.
struct Checked {
    /// The state the rail took, when it took one.
    std::optional<BlockChange> change;
    /// Whether the pass ran at all. parity: `checkPressed` ends in `updateNeighbourForOutputSignal`
    /// **whatever** it decided, so a comparator reading a container minecart parked on the rail is
    /// told again on every pass and follows what is inside the cart. Gated on the run and not on the
    /// change, which is the whole point.
    bool ran = false;
};

/// Who is asking, because the jar gates the two callers differently.
enum class Caller : std::uint8_t {
    /// parity: `entityInside`, which asks only while the rail is **not** powered.
    Entity,
    /// parity: `tick`, which asks only while it **is**.
    ScheduledTick,
};

/// Looks at what is standing on the rail and answers the change, if it changed.
///
/// parity: `checkPressed` is called both by a cart arriving and by the rail's own scheduled tick,
/// which is why this takes the whole answer rather than a direction.
[[nodiscard]] Checked check(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            std::span<entity::ServerEntity* const> entities, Caller caller);

/// What a comparator beside the rail reads: how full the container minecart standing on it is.
///
/// parity: `DetectorRailBlock.getAnalogOutputSignal`, which answers only while the rail is pressed.
/// The command block minecart's success count is not here: this build has no command block minecart.
[[nodiscard]] std::int32_t comparatorSignal(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::span<entity::ServerEntity* const> entities);

} // namespace detectorRails

} // namespace basalt::server::runtime::world
