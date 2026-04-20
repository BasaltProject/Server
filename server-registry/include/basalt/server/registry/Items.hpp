#pragma once

#include <basalt/wire/common/Identifier.hpp>

namespace basalt::wire::nbt {
class NbtCompound;
}

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::registry {
struct ArmourPiece {
    std::string_view slot;
    float armour = 0.0F;
    float toughness = 0.0F;
    float knockbackResistance = 0.0F;
};

struct WeaponStats {
    double attackDamage = 0.0;
    double attackSpeed = 0.0;
};

struct ItemStackingEntry {
    std::int32_t maxStackSize = 0;
    std::int32_t cooldownTicks = 0;
};

// parity: the `minecraft:attribute_modifiers` of every wearable item.
namespace itemArmour {
[[nodiscard]] const ArmourPiece* of(const wire::common::Identifier& item);
} // namespace itemArmour

// parity: the `mainhand` half of every item's `minecraft:attribute_modifiers`.
namespace itemWeapons {
// parity: `Attributes.ATTACK_DAMAGE`'s own default for a player.
inline constexpr double kBaseAttackDamage = 1.0;
// parity: `Attributes.ATTACK_SPEED`'s default: four swings a second with an empty hand.
inline constexpr double kBaseAttackSpeed = 4.0;

[[nodiscard]] const WeaponStats* of(const wire::common::Identifier& item);
} // namespace itemWeapons

// parity: `minecraft:max_stack_size` and `minecraft:use_cooldown`.
namespace itemStacking {
// parity: `Item.Properties`' own defaults, which most items never change.
inline constexpr std::int32_t kDefaultStackSize = 64;

[[nodiscard]] const ItemStackingEntry* of(const wire::common::Identifier& item);

[[nodiscard]] std::int32_t maxStackSize(const wire::common::Identifier& item);

// parity: `ItemStack.getMaxStackSize`.
[[nodiscard]] std::int32_t maxStackSize(const wire::common::Identifier& item, const wire::nbt::NbtCompound& components);

[[nodiscard]] std::int32_t maxStackSize(std::string_view item, const wire::nbt::NbtCompound& components);

[[nodiscard]] std::int32_t cooldownTicks(const wire::common::Identifier& item);
} // namespace itemStacking

// parity: an item with no entry has no `minecraft:max_damage` component in vanilla either.
namespace itemDurability {
struct Entry {
    std::int32_t maxDamage = 0;
    std::int32_t damagePerBlock = 0;
    std::int32_t damagePerAttack = 0;
};

[[nodiscard]] const Entry* of(const wire::common::Identifier& item);

[[nodiscard]] std::int32_t maxDamageOf(const wire::common::Identifier& item);

[[nodiscard]] bool isDamageable(const wire::common::Identifier& item);
} // namespace itemDurability

// parity: `BlockItem.getBlock`.
namespace itemBlocks {
[[nodiscard]] wire::common::Identifier of(const wire::common::Identifier& item);

// parity: `Block.asItem`.
[[nodiscard]] wire::common::Identifier itemOf(const wire::common::Identifier& block);
} // namespace itemBlocks

// parity: `Item.getCraftingRemainder`.
namespace craftingRemainders {
[[nodiscard]] const wire::common::Identifier* of(const wire::common::Identifier& item);
} // namespace craftingRemainders

// parity: `DataComponents.PROVIDES_TRIM_MATERIAL`.
namespace trimMaterials {
[[nodiscard]] const wire::common::Identifier* of(const wire::common::Identifier& item);
} // namespace trimMaterials

// parity: `FuelValues.vanillaBurnTimes`.
namespace fuels {
[[nodiscard]] std::int32_t burnTicks(const wire::common::Identifier& item);

[[nodiscard]] bool isFuel(const wire::common::Identifier& item);
} // namespace fuels

// parity: `DyeColor`'s own `textureDiffuseColor`.
namespace dyeColours {
[[nodiscard]] std::optional<std::int32_t> of(const wire::common::Identifier& item);
} // namespace dyeColours

// parity: `WeatheringCopper.NEXT_BY_BLOCK`.
namespace weathering {
[[nodiscard]] std::optional<std::string_view> next(std::string_view block);

[[nodiscard]] std::optional<std::string_view> previous(std::string_view block);

[[nodiscard]] std::optional<std::int32_t> stageOf(std::string_view block);

[[nodiscard]] bool weathers(std::string_view block);

// parity: `Block.shouldChangedStateKeepBlockEntity`.
[[nodiscard]] bool keepsBlockEntity(std::string_view from, std::string_view to);
} // namespace weathering
} // namespace basalt::server::registry
