#pragma once

#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {
struct DamageSource;
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::scoreboard {
class Teams;
} // namespace basalt::server::runtime::scoreboard

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
class RealEnchantments;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::player {

class PlayerSession;

struct DamageContext {
    PlayerRegistry* registry = nullptr;
    const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    const scoreboard::Teams* teams = nullptr;
    std::function<world::LoadedWorld*()> world;
    std::function<std::optional<float>(const PlayerSession& player, float amount, std::string_view cause,
                                       const std::optional<std::int32_t>& sourceEntityId)>
        damaged;
    std::function<std::optional<float>(const PlayerSession& player, float amount)> healed;
    std::function<void(const PlayerSession& player, std::int32_t slot)> slotChanged;
    std::function<void(PlayerSession& player, std::int32_t cooldownTicks)> disableBlocking;
    DeathContext death;
    world::AdvancementTriggers* triggers = nullptr;
};

[[nodiscard]] bool shieldFaces(const PlayerSession& session, double fromX, double fromZ);

[[nodiscard]] float damageAfterProtection(const PlayerSession& session, float amount,
                                          const std::optional<wire::common::Identifier>& damageType,
                                          const DamageContext& context, float* resisted = nullptr,
                                          float armorFractionAdd = 0.0F);

void hurtPlayer(PlayerSession& session, float amount, std::string_view deathMessageId,
                const std::optional<wire::common::Identifier>& damageType,
                const std::optional<entity::DamageSource>& source, const DamageContext& context);

void healPlayer(PlayerSession& session, float amount, const DamageContext& context);

} // namespace basalt::server::runtime::player
