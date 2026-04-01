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

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
class RealEnchantments;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::player {

class PlayerSession;

/// What being hurt needs of the server, without needing the connection it happened on. Armour,
/// absorption, invulnerability and the arithmetic are the same on every edition; only the flash, the
/// shove and the hearts are callbacks. Apart from `CombatHandler` so a player with no
/// `ClientSession` can still be hurt.
struct DamageContext {
    PlayerRegistry* registry = nullptr;
    const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    /// Where the player is, for the vibration a hit makes. Answers null when they are nowhere.
    std::function<world::LoadedWorld*()> world;
    /// What a plugin is asked. Answering nothing stops the hit, or the heal, entirely.
    std::function<std::optional<float>(const PlayerSession& player, float amount, std::string_view cause,
                                       const std::optional<std::int32_t>& sourceEntityId)>
        damaged;
    std::function<std::optional<float>(const PlayerSession& player, float amount)> healed;
    /// Tells the player one slot of theirs changed, however their edition says it.
    std::function<void(const PlayerSession& player, std::int32_t slot)> slotChanged;
    /// Lowers the shield an axe knocked away, and greys it out for that many ticks.
    std::function<void(PlayerSession& player, std::int32_t cooldownTicks)> disableBlocking;
    /// What dying needs, for the hit that takes the last half heart.
    DeathContext death;
    /// Where `entity_hurt_player` and `entity_killed_player` go.
    world::AdvancementTriggers* triggers = nullptr;
};

/// Whether a raised shield faces a hit coming from there, which is what turns an arrow back.
/// parity: `LivingEntity.resolveBlockingItem` with `BlocksAttacks`' own ninety degrees.
[[nodiscard]] bool shieldFaces(const PlayerSession& session, double fromX, double fromZ);

/// What is left of a hit after armour, resistance, protection and the tags that bypass them.
[[nodiscard]] float damageAfterProtection(const PlayerSession& session, float amount,
                                          const std::optional<wire::common::Identifier>& damageType,
                                          const DamageContext& context, float* resisted = nullptr);

/// Hurts a player who is neither dead nor invulnerable: the cooldown, the armour, the absorption,
/// the flash everyone sees, the shove, and dying when it takes the last of them.
void hurtPlayer(PlayerSession& session, float amount, std::string_view deathMessageId,
                const std::optional<wire::common::Identifier>& damageType,
                const std::optional<entity::DamageSource>& source, const DamageContext& context);

/// Heals a player who is not dead, up to their maximum and no further.
void healPlayer(PlayerSession& session, float amount, const DamageContext& context);

} // namespace basalt::server::runtime::player
