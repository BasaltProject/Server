#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

/// parity: `Player.causeFoodExhaustion`, which asks `EntityExhaustionEvent` and skips invulnerable players.
void causeExhaustion(PlayerSession& session, float amount, std::string_view reason);

/// The exhaustion a hit of this damage type costs, from the damage type registry.
[[nodiscard]] float damageExhaustion(const wire::common::Identifier& damageType);

} // namespace basalt::server::runtime::player
