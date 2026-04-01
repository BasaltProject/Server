#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// Gliding on an elytra: when it may start, when it stops, and the wear on the wings.
///
/// parity: `Player.tryToStartFallFlying` and `LivingEntity.updateFallFlying`. The flying itself is
/// the client's; the server only keeps the flag and wears the elytra.
namespace gliding {

/// parity: `LivingEntity.canGlide`: in the air, riding nothing, not levitating, wings on.
[[nodiscard]] bool canGlide(const PlayerSession& player);

/// parity: `ServerboundPlayerCommandPacket.START_FALL_FLYING`: start, or else stop.
void startFallFlying(PlayerSession& player);

/// parity: `LivingEntity.tick`'s `fallFlyTicks` and `aiStep`'s `updateFallFlying`.
void tick(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world);

} // namespace gliding

} // namespace basalt::server::runtime::player
