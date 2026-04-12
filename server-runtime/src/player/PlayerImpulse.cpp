#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <algorithm>

namespace basalt::server::runtime::player::impulse {

void onExplosionHit(PlayerSession& player, bool windCharge) {
    if (!windCharge) {
        player.impulseGraceTicks = 0;
        return;
    }
    player.impulseGraceTicks = std::max(player.impulseGraceTicks, kGraceTicks);
    player.impulseImpact = std::array<double, 3>{player.x, player.y, player.z};
}

void tick(PlayerSession& player) {
    if (player.impulseGraceTicks > 0) {
        --player.impulseGraceTicks;
    }
}

void tryReset(PlayerSession& player) {
    if (player.impulseGraceTicks == 0) {
        reset(player);
    }
}

void reset(PlayerSession& player) {
    player.impulseGraceTicks = 0;
    player.impulseImpact.reset();
}

double effectiveFallDistance(PlayerSession& player, double fallDistance, double landingY) {
    if (!player.impulseImpact.has_value()) {
        return fallDistance;
    }
    const double effective = std::min(fallDistance, (*player.impulseImpact)[1] - landingY);
    if (effective <= 0.0) {
        reset(player);
    } else {
        tryReset(player);
    }
    return effective;
}

} // namespace basalt::server::runtime::player::impulse
