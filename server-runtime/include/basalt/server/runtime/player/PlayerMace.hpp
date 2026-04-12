#pragma once

#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {

/// The mace's smash: a hit on the way down that is worth more the further the fall.
///
/// parity: `MaceItem`, with density's `smash_damage_per_fallen_block` and the `mace_smash` damage
/// type it switches to. Breach rides on the damage source, as `armor_effectiveness`.
namespace mace {

/// parity: `MaceItem.SMASH_ATTACK_FALL_THRESHOLD`.
inline constexpr double kSmashThreshold = 1.5;

[[nodiscard]] bool isMace(const std::optional<storage::StoredItemStack>& weapon);

/// parity: `MaceItem.canSmashAttack`: falling more than a block and a half, and not gliding.
[[nodiscard]] bool canSmash(const PlayerSession& attacker);

/// Whether this blow is a smash: a mace in hand and the fall to go with it.
[[nodiscard]] bool smashes(const PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon);

/// parity: `MaceItem.getAttackDamageBonus`, the fall's own bonus plus density's per block.
[[nodiscard]] float damageBonus(const PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon,
                                const world::weaponDamage::DefinitionOf& definitionOf);

/// What the victim is, as the smash's shock wave reads it.
struct Struck {
    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool onGround = false;
    /// The uuid the tame animals near it are asked about, `MaceItem`'s own quirk.
    std::string uuid;
};

/// parity: `MaceItem.hurtEnemy`: the attacker stopped in the air and spared the fall, the sound,
/// and everything around the victim thrown out.
void hurtEnemy(const pvp::Context& context, world::LoadedWorld& world, PlayerSession& attacker, const Struck& struck);

/// parity: `MaceItem.postHurtEnemy`, the fall forgotten.
void postHurtEnemy(PlayerSession& attacker);

/// What the weapon's `armor_effectiveness` adds to the victim's armour share: breach.
[[nodiscard]] float armorFractionAdd(const std::optional<storage::StoredItemStack>& weapon,
                                     const world::weaponDamage::DefinitionOf& definitionOf);

} // namespace mace

} // namespace basalt::server::runtime::player
