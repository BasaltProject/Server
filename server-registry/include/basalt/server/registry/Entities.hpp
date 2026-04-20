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
// parity: `DefaultAttributes`.
namespace entityAttributes {
inline constexpr std::string_view kMaxHealth = "max_health";
inline constexpr std::string_view kMovementSpeed = "movement_speed";
inline constexpr std::string_view kFollowRange = "follow_range";
inline constexpr std::string_view kAttackDamage = "attack_damage";
inline constexpr std::string_view kArmor = "armor";
inline constexpr std::string_view kArmorToughness = "armor_toughness";
inline constexpr std::string_view kKnockbackResistance = "knockback_resistance";
inline constexpr std::string_view kSafeFallDistance = "safe_fall_distance";

[[nodiscard]] std::optional<double> baseValue(const wire::common::Identifier& type, std::string_view attribute);

[[nodiscard]] const std::map<std::string, double>& of(const wire::common::Identifier& type);

[[nodiscard]] bool isLiving(const wire::common::Identifier& type);
} // namespace entityAttributes

namespace attributeIds {
[[nodiscard]] std::optional<std::int32_t> protocolId(std::string_view name);

[[nodiscard]] bool exists(std::string_view name);

[[nodiscard]] std::vector<std::string_view> all();
} // namespace attributeIds

// parity: `PotionBrewing.bootstrap`.
namespace brewing {
// parity: `#minecraft:brewing_fuel`, which is blaze powder and nothing else.
[[nodiscard]] const wire::common::Identifier& fuel();

// parity: `BrewingStandBlockEntity`.
inline constexpr std::int32_t kFuelUses = 20;
inline constexpr std::int32_t kBrewTime = 400;

struct Brew {
    wire::common::Identifier bottle;
    std::optional<std::string> potion;
};

// parity: `PotionBrewing.isIngredient`.
[[nodiscard]] bool isIngredient(const wire::common::Identifier& item);

// parity: `PotionBrewing.mix`.
[[nodiscard]] std::optional<Brew> mix(const wire::common::Identifier& bottle, const std::optional<std::string>& potion,
                                      const wire::common::Identifier& ingredient);
} // namespace brewing

// parity: `EntityType`'s own builder values.
namespace entityTypeShapes {
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

[[nodiscard]] double spawnWidthOf(const wire::common::Identifier& type);

[[nodiscard]] double spawnHeightOf(const wire::common::Identifier& type);

// parity: `MobCategory`, which is what a spawn cap counts and what despawn rules read.
[[nodiscard]] std::string_view categoryOf(const wire::common::Identifier& type);

[[nodiscard]] bool isFireImmune(const wire::common::Identifier& type);

// parity: the builder `immuneTo` block tags consulted by `EntityType.isBlockDangerous`.
[[nodiscard]] bool isImmuneTo(const wire::common::Identifier& type, std::string_view block);

// parity: `EntityType.canSpawnFarFromPlayer`.
[[nodiscard]] bool canSpawnFarFromPlayer(const wire::common::Identifier& type);
} // namespace entityTypeShapes
} // namespace basalt::server::registry
