#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <functional>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

/// parity: `CactusBlock.entityInside`, `MagmaBlock.stepOn` and `BaseFireBlock.entityInside` for a player.
void tickBlockContact(PlayerSession& session, const entity::entityCollision::BlockAt& blockAt,
                      random::RandomSource& random,
                      const std::function<void(float amount, std::string_view deathMessageId,
                                               const wire::common::Identifier& damageType)>& hurt,
                      bool magmaHurtsSneaking = false, float stonecutterDamage = 0.0F);

} // namespace basalt::server::runtime::player
