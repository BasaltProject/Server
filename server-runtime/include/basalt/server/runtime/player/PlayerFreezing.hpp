#pragma once

#include <basalt/server/runtime/entity/EntityCollision.hpp>

#include <functional>

namespace basalt::server::runtime::player {

class PlayerSession;

/// parity: `LivingEntity.aiStep`'s freezing step and `InsideBlockEffectType.FREEZE`; `hurt` takes the freeze damage.
void tickFreezing(PlayerSession& session, const entity::entityCollision::BlockAt& blockAt, bool freezeDamage,
                  const std::function<void(float amount)>& hurt);

/// parity: `Entity.getPercentFrozen`.
[[nodiscard]] float percentFrozen(const PlayerSession& session);

} // namespace basalt::server::runtime::player
