#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

class PlayerSession;

namespace gliding {

/// parity: `LivingEntity.canGlide`: in the air, riding nothing, not levitating, wings on.
[[nodiscard]] bool canGlide(const PlayerSession& player);

/// parity: `ServerboundPlayerCommandPacket.START_FALL_FLYING`: start, or else stop.
void startFallFlying(PlayerSession& player);

void tick(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world);

} // namespace gliding

} // namespace basalt::server::runtime::player
