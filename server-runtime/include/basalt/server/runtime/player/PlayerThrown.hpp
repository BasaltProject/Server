#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// Throws what is in that slot, if it is something this build throws: an ender pearl and nothing
/// else yet. Answers whether it was thrown.
///
/// Shared because the arc is the same from both doors: a Java client asks with a use in the air,
/// and a Bedrock one with the same use on the block it happened to be pointing at.
bool throwItemFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, std::int32_t slot);

/// Whether a plugin lets this shot go. Unset means nobody objects.
using ShotGate =
    std::function<bool(const PlayerSession& player, const wire::common::Identifier& projectile, float power)>;

/// parity: `BowItem.getPowerForTime`, the square of the draw.
[[nodiscard]] float bowPowerOf(std::int32_t drawnTicks);

/// Lets a drawn bow go: the arrow, its speed, the noise and the arrow it spends. `drawnTicks` is how
/// long it was held, which is what decides all three.
///
/// Shared because the arrow flies the same either way. What differs is the clock: the Java session
/// counts the draw itself, and a Bedrock client draws on its own and says when it let go.
void shootBowFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                 std::int32_t drawnTicks, const ShotGate& allowed);

} // namespace basalt::server::runtime::player
