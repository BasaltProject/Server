#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What the enchantments on both sides of a hit do once it has landed.
///
/// parity: `EnchantmentHelper.doPostAttackEffectsWithItemSource` over each enchantment's
/// `minecraft:post_attack` entries: the victim's worn and held items first, for the entries it is
/// `enchanted` as (thorns), then the weapon, for the attacker's (fire aspect, bane of arthropods).
namespace postAttack {

/// Which side of the hit an effect lands on. `damaging_entity` is the attacker for a melee blow.
enum class Side : std::uint8_t { kVictim, kAttacker };

/// A stack worn or held, and the equipment slot it is in: `mainhand`, `offhand`, `feet`, `legs`,
/// `chest`, `head` or `body`.
struct Equipped {
    std::int32_t slot = 0;
    std::string_view equipmentSlot;
    storage::StoredItemStack stack;
};

struct Ignite {
    Side target = Side::kVictim;
    std::int32_t ticks = 0;
};

/// parity: `DamageEntity`, whose source is the owner of the enchanted item.
struct Damage {
    Side target = Side::kAttacker;
    wire::common::Identifier type;
    float amount = 0.0F;
};

struct MobEffect {
    Side target = Side::kVictim;
    wire::common::Identifier effect;
    std::int32_t ticks = 0;
    std::int32_t amplifier = 0;
};

/// parity: `ChangeItemDamage` on the enchanted stack itself.
struct ItemWear {
    Side owner = Side::kVictim;
    std::int32_t slot = 0;
    std::int32_t amount = 0;
};

/// parity: `ExplodeEffect`, a blast at whoever it lands on: wind burst's gust.
struct Explosion {
    Side target = Side::kAttacker;
    float radius = 0.0F;
    std::optional<float> knockbackMultiplier;
    /// `block_interaction` is `trigger`: buttons, levers and the like answer it.
    bool triggersBlocks = false;
    std::string immuneBlocks;
    std::string smallParticle;
    std::string largeParticle;
    std::string sound;
};

/// Everything the entries that passed their requirements asked for, in the order they ran.
struct Outcome {
    std::vector<Ignite> ignites;
    std::vector<Damage> damages;
    std::vector<MobEffect> effects;
    std::vector<ItemWear> wear;
    std::vector<Explosion> explosions;
};

struct Hit {
    /// The victim's equipment, in `EquipmentSlot` order.
    std::vector<Equipped> victimEquipment;
    std::optional<Equipped> weapon;
    /// The victim as `entity_properties` on `this` reads it.
    const weaponDamage::Victim* victim = nullptr;
    /// parity: `DamageSource.isDirect`, which a melee blow is.
    bool direct = true;
    /// The attacker as `entity_properties` on `attacker` and `direct_attacker` reads it.
    double attackerFallDistance = 0.0;
    bool attackerFlying = false;
};

[[nodiscard]] Outcome of(const Hit& hit, const weaponDamage::DefinitionOf& definitionOf, random::RandomSource& random);

} // namespace postAttack

} // namespace basalt::server::runtime::world
