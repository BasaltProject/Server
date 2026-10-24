#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/PostAttackEffects.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
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

class PlayerRegistry;
class PlayerSession;
struct BreakContext;

namespace pvp {

/// parity: `Entity.isAlliedTo` once plugins have added their own rules; false while nobody answers.
[[nodiscard]] bool alliedByRules(const PlayerSession& attacker, std::int32_t otherId);

[[nodiscard]] bool canHarm(const PlayerSession& victim, const PlayerSession& attacker, bool pvpAllowed,
                           const scoreboard::Teams* teams);

[[nodiscard]] bool criticalPossible(const PlayerSession& attacker, world::LoadedWorld& world, bool falling);

struct Context {
    PlayerRegistry* registry = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
    const scoreboard::Teams* teams = nullptr;
    const BreakContext* blocks = nullptr;
    std::function<void(entity::ServerEntity& mob, bool died)> sweptMob;
    std::function<std::int64_t()> nextSeed;
    std::function<void(world::LoadedWorld& world, entity::ServerEntity& mob, const world::ActiveEffect& effect,
                       std::int32_t sourceId)>
        addMobEffect;
    std::function<void(world::LoadedWorld& world, const world::postAttack::Explosion& blast, double x, double y,
                       double z)>
        explode;
};

struct Swing {
    float strength = 1.0F;
    float base = 0.0F;
    bool falling = false;
    std::optional<storage::StoredItemStack> weapon;
    std::int32_t weaponSlot = 0;
    bool slowEnoughToSweep = true;
};

bool attack(PlayerSession& attacker, PlayerSession& victim, world::LoadedWorld& world, const Context& context,
            const Swing& swing);

/// parity: `EnchantmentHelper.doPostAttackEffectsWithItemSource` after a player's blow on `victim`.
void postAttack(const Context& context, world::LoadedWorld& world, PlayerSession& attacker, PlayerSession* victim,
                entity::ServerEntity* mob, const std::optional<storage::StoredItemStack>& weapon,
                std::int32_t weaponSlot);

void itemAttackInteraction(const Context& context, world::LoadedWorld& world, PlayerSession& attacker,
                           PlayerSession* victim, entity::ServerEntity* mob,
                           const std::optional<storage::StoredItemStack>& weapon, std::int32_t weaponSlot);

[[nodiscard]] float knockbackOf(const Context& context, const PlayerSession& attacker,
                                const std::optional<storage::StoredItemStack>& weapon);

void knockMob(PlayerSession& attacker, entity::ServerEntity& mob, float power);

void sweep(PlayerSession& attacker, world::LoadedWorld& world, const Context& context, std::int32_t targetId,
           const entity::Aabb& targetBox, float baseDamage, float strength,
           const std::optional<storage::StoredItemStack>& weapon);

void sendMotion(PlayerSession& player, const std::array<double, 3>& motion, float fromYaw);

void showDamageIndicator(const std::vector<PlayerSession*>& players, double x, double y, double z, float lost);

} // namespace pvp

} // namespace basalt::server::runtime::player
