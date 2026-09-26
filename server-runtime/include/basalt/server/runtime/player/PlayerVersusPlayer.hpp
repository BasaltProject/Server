#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
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

/// A player's melee blow on another player, and the sweep that can catch both mobs and players.
/// Edition-agnostic: the Java and the Bedrock door both swing through here.
namespace pvp {

/// parity: `ServerPlayer.canHarmPlayer`: nothing while the `pvp` rule is off, then `Player`'s
/// `doTeamsAllowDamage`, which lets a team hurt its own only with friendly fire on.
[[nodiscard]] bool canHarm(const PlayerSession& victim, const PlayerSession& attacker, bool pvpAllowed,
                           const scoreboard::Teams* teams);

/// parity: `canCriticalAttack` less the sprint, which the swing checks: falling, and neither on a
/// ladder, in water, blind nor riding.
[[nodiscard]] bool criticalPossible(const PlayerSession& attacker, world::LoadedWorld& world, bool falling);

/// What a blow needs of the server around it.
struct Context {
    PlayerRegistry* registry = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
    const scoreboard::Teams* teams = nullptr;
    /// What wears the attacker's weapon and the victim's armour, triggers and statistics included.
    const BreakContext* blocks = nullptr;
    /// Tells everyone that a mob the sweep caught was hurt, and kills it when the blow did.
    std::function<void(entity::ServerEntity& mob, bool died)> sweptMob;
    std::function<std::int64_t()> nextSeed;
};

/// The swing as the attacker's side worked it out.
struct Swing {
    /// parity: `getAttackStrengthScale(0.5F)`.
    float strength = 1.0F;
    /// The attack damage attribute, or the riptide's own damage while spinning.
    float base = 0.0F;
    /// Whether a critical is possible at all: falling, off the ground and not in water or riding.
    bool falling = false;
    std::optional<storage::StoredItemStack> weapon;
    std::int32_t weaponSlot = 0;
    /// parity: `isSweepAttack`'s `getKnownMovement` under the speed limit.
    bool slowEnoughToSweep = true;
};

/// parity: `Player.attack` on a `ServerPlayer`: the charge, the critical, the sprint push, the
/// sweep, the sounds, the weapon's wear and post-attack enchantments on both sides, the damage
/// statistic and the dark hearts. Answers whether the blow landed.
bool attack(PlayerSession& attacker, PlayerSession& victim, world::LoadedWorld& world, const Context& context,
            const Swing& swing);

/// parity: `doSweepAttack`: everything living in the target's box grown by one and a quarter,
/// within three blocks of the attacker and not on their team, mobs and players alike.
/// `baseDamage` is the charged base the blow itself used, before any critical.
void sweep(PlayerSession& attacker, world::LoadedWorld& world, const Context& context, std::int32_t targetId,
           const entity::Aabb& targetBox, float baseDamage, float strength,
           const std::optional<storage::StoredItemStack>& weapon);

/// parity: `causeExtraKnockback` on a `ServerPlayer`: the push goes out as their motion, since the
/// client is the one that moves them. `fromYaw` is where the hit came from, for Bedrock's tilt.
void sendMotion(PlayerSession& player, const std::array<double, 3>& motion, float fromYaw);

/// parity: `damageStatsAndHearts`'s dark hearts, for a hit that took more than a heart.
void showDamageIndicator(const std::vector<PlayerSession*>& players, double x, double y, double z, float lost);

} // namespace pvp

} // namespace basalt::server::runtime::player
