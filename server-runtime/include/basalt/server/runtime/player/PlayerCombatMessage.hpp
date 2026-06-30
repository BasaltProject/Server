#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/wire/text/Component.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

/// parity: `CombatTracker.getDeathMessage`, read over the records the session kept.
[[nodiscard]] wire::text::Component trackedDeathMessage(const PlayerSession& session, std::string_view deathMessageId,
                                                        const std::optional<entity::DamageSource>& source,
                                                        const DeathContext& context);

} // namespace basalt::server::runtime::player
