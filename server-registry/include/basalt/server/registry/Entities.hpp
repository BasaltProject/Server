#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::registry {

/// What each kind of mob starts with: its health, how fast it walks, how far it notices you.
///
/// parity: `DefaultAttributes`, which each entity class fills in through its own `createAttributes`.
/// A cow is not a zombie is not a rabbit, and every one of those numbers is observable.
namespace entityAttributes {

inline constexpr std::string_view kMaxHealth = "max_health";
inline constexpr std::string_view kMovementSpeed = "movement_speed";
inline constexpr std::string_view kFollowRange = "follow_range";
inline constexpr std::string_view kAttackDamage = "attack_damage";
inline constexpr std::string_view kArmor = "armor";
inline constexpr std::string_view kArmorToughness = "armor_toughness";
inline constexpr std::string_view kKnockbackResistance = "knockback_resistance";
inline constexpr std::string_view kSafeFallDistance = "safe_fall_distance";

/// The base value this type has for `attribute`, or absent when it has no such attribute. The name
/// may carry the `minecraft:` prefix or not.
[[nodiscard]] std::optional<double> baseValue(const wire::common::Identifier& type, std::string_view attribute);

[[nodiscard]] const std::map<std::string, double>& of(const wire::common::Identifier& type);

/// Whether this type is alive at all, which is what having attributes means.
[[nodiscard]] bool isLiving(const wire::common::Identifier& type);

} // namespace entityAttributes

/// The `minecraft:attribute` registry: every attribute and the number it travels as.
///
/// A protocol fact, taken from the jar's own registry report, like the block and item tables beside
/// it. It is here rather than written where it is used because it was written there once already
/// and one of the six numbers was wrong: `attack_speed` went out as `attack_knockback`, which is a
/// mistake nobody spots by reading the code.
namespace attributeIds {

/// Accepts `max_health` and `minecraft:max_health` alike, since a plugin will write both.
[[nodiscard]] std::optional<std::int32_t> protocolId(std::string_view name);

[[nodiscard]] bool exists(std::string_view name);

[[nodiscard]] std::vector<std::string_view> all();

} // namespace attributeIds

/// What a brewing stand can make.
///
/// parity: `PotionBrewing.bootstrap`, which vanilla builds in code rather than in data, so the table
/// is that call's own answer read back out of it.
namespace brewing {

/// parity: `#minecraft:brewing_fuel`, which is blaze powder and nothing else.
[[nodiscard]] const wire::common::Identifier& fuel();

/// parity: `BrewingStandBlockEntity`: twenty brews out of one powder, four hundred ticks each.
inline constexpr std::int32_t kFuelUses = 20;
inline constexpr std::int32_t kBrewTime = 400;

/// One bottle and what is in it.
struct Brew {
    wire::common::Identifier bottle;
    /// Absent for a bottle of plain water, which is where every chain starts.
    std::optional<std::string> potion;
};

/// Whether `item` is an ingredient any brew uses at all. parity: `PotionBrewing.isIngredient`.
[[nodiscard]] bool isIngredient(const wire::common::Identifier& item);

/// What `bottle` holding `potion` becomes when `ingredient` is dropped on it, or absent when that
/// pair brews nothing. parity: `PotionBrewing.mix`.
[[nodiscard]] std::optional<Brew> mix(const wire::common::Identifier& bottle, const std::optional<std::string>& potion,
                                      const wire::common::Identifier& ingredient);

} // namespace brewing

/// How big each entity type is, and what it counts as when the world decides what to spawn.
///
/// parity: `EntityType`'s own builder values. The box is what collides with the world, the category
/// is what a spawn cap counts, and the tracking range is how far a client is told about it.
namespace entityTypeShapes {

/// One entity type's box, spawn category, tracking and the two flags a spawn rule reads.
struct Shape {
    std::string_view entity;
    float width = 0.0F;
    float height = 0.0F;
    std::string_view category;
    std::int32_t trackingRange = 0;
    std::int32_t updateInterval = 0;
    bool fireImmune = false;
    bool farFromPlayer = false;
};

[[nodiscard]] const Shape* of(const wire::common::Identifier& type);

[[nodiscard]] double widthOf(const wire::common::Identifier& type);

[[nodiscard]] double heightOf(const wire::common::Identifier& type);

/// Width of `EntityType.getSpawnAABB`, including the cube mobs' builder-only safety scale.
[[nodiscard]] double spawnWidthOf(const wire::common::Identifier& type);

/// Height of `EntityType.getSpawnAABB`, including the cube mobs' builder-only safety scale.
[[nodiscard]] double spawnHeightOf(const wire::common::Identifier& type);

/// parity: `MobCategory`, which is what a spawn cap counts and what despawn rules read.
[[nodiscard]] std::string_view categoryOf(const wire::common::Identifier& type);

[[nodiscard]] bool isFireImmune(const wire::common::Identifier& type);

/// parity: the builder `immuneTo` block tags consulted by `EntityType.isBlockDangerous`.
[[nodiscard]] bool isImmuneTo(const wire::common::Identifier& type, std::string_view block);

/// parity: `EntityType.canSpawnFarFromPlayer`, true by default for an unknown or custom misc type.
[[nodiscard]] bool canSpawnFarFromPlayer(const wire::common::Identifier& type);

} // namespace entityTypeShapes

} // namespace basalt::server::registry
