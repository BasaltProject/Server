#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::registry {

/// What one piece of armour is worth, straight from its own attribute modifiers.
struct ArmourPiece {
    std::string_view slot;
    float armour = 0.0F;
    float toughness = 0.0F;
    float knockbackResistance = 0.0F;
};

/// What holding an item adds to the hand: how hard it hits and how fast it swings back.
struct WeaponStats {
    double attackDamage = 0.0;
    double attackSpeed = 0.0;
};

/// How high one item stacks, and how long it waits between uses.
struct ItemStackingEntry {
    std::int32_t maxStackSize = 0;
    std::int32_t cooldownTicks = 0;
};

/// parity: the `minecraft:attribute_modifiers` of every wearable item.
namespace itemArmour {

[[nodiscard]] const ArmourPiece* of(const wire::common::Identifier& item);

} // namespace itemArmour

/// parity: the `mainhand` half of every item's `minecraft:attribute_modifiers`.
///
/// Both numbers are modifiers, not totals: a player swings at one damage and four speed with nothing
/// in hand, and the item is added to that.
namespace itemWeapons {

/// parity: `Attributes.ATTACK_DAMAGE`'s own default for a player.
inline constexpr double kBaseAttackDamage = 1.0;
/// parity: `Attributes.ATTACK_SPEED`'s default: four swings a second with an empty hand.
inline constexpr double kBaseAttackSpeed = 4.0;

[[nodiscard]] const WeaponStats* of(const wire::common::Identifier& item);

} // namespace itemWeapons

/// The two item components that decide how a stack behaves outside a recipe.
///
/// parity: `minecraft:max_stack_size` and `minecraft:use_cooldown`, both written in the item's own
/// component file: an ender pearl waits a second because its file says so, not because any class
/// mentions ender pearls.
namespace itemStacking {

/// parity: `Item.Properties`' own defaults, which most items never change.
inline constexpr std::int32_t kDefaultStackSize = 64;

[[nodiscard]] const ItemStackingEntry* of(const wire::common::Identifier& item);

[[nodiscard]] std::int32_t maxStackSize(const wire::common::Identifier& item);

/// How long the item is greyed out after a use, or zero when it is never greyed out.
[[nodiscard]] std::int32_t cooldownTicks(const wire::common::Identifier& item);

} // namespace itemStacking

/// Which items wear out, and how fast.
///
/// parity: an item with no entry has no `minecraft:max_damage` component in vanilla either, and
/// vanilla's `ItemStack.hurtAndBreak` returns immediately for one, so it never wears rather than
/// wearing slowly.
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

/// Which block an item puts down.
///
/// parity: `BlockItem.getBlock`. Nearly every item shares its name with its block, and the dozen
/// that do not are the ones that made this necessary: redstone dust is `redstone_wire`, string is
/// `tripwire`, and every seed is called something else from the crop it grows.
namespace itemBlocks {

/// The block `item` places, which is `item` itself unless the table says otherwise.
[[nodiscard]] wire::common::Identifier of(const wire::common::Identifier& item);

/// The item `block` is picked up as, which is `block` itself unless the table says otherwise.
///
/// parity: `Block.asItem`. Without it, middle-clicking dust, a crop or a cocoa pod gives nothing,
/// because no item is called `redstone_wire`.
[[nodiscard]] wire::common::Identifier itemOf(const wire::common::Identifier& block);

} // namespace itemBlocks

/// What an item leaves behind in the grid when it is crafted with.
///
/// parity: `Item.getCraftingRemainder`. Five items have one, and they are the reason a cake gives
/// the buckets back instead of eating them.
namespace craftingRemainders {

[[nodiscard]] const wire::common::Identifier* of(const wire::common::Identifier& item);

} // namespace craftingRemainders

/// Which trim material each item provides.
///
/// parity: `DataComponents.PROVIDES_TRIM_MATERIAL`, a default component on the ingot or gem rather
/// than a list anywhere. An item absent from here cannot trim armour, which is the whole rule a
/// smithing trim recipe checks.
namespace trimMaterials {

[[nodiscard]] const wire::common::Identifier* of(const wire::common::Identifier& item);

} // namespace trimMaterials

/// What burns in a furnace and for how long.
///
/// parity: `FuelValues.vanillaBurnTimes`. Vanilla builds this table in code and not in data, so the
/// generated list is that call's own answer, tags already flattened and the non-flammable woods
/// already taken back out.
namespace fuels {

/// How long `item` keeps a furnace lit, or 0 when it is not fuel.
[[nodiscard]] std::int32_t burnTicks(const wire::common::Identifier& item);

[[nodiscard]] bool isFuel(const wire::common::Identifier& item);

} // namespace fuels

/// The sixteen dyes and the colour each one carries.
///
/// parity: `DyeColor`'s own `textureDiffuseColor`, which is what `DyedItemColor.applyDyes` averages
/// when leather armour or a firework star is dyed. A hand table rather than a generated one: sixteen
/// numbers that have not moved in a decade, and the generator has no way to name them.
namespace dyeColours {

/// The colour `item` dyes with, or absent when it is not a dye.
[[nodiscard]] std::optional<std::int32_t> of(const wire::common::Identifier& item);

} // namespace dyeColours

/// What copper turns into as it weathers, and how far along each block already is.
///
/// parity: `WeatheringCopper.NEXT_BY_BLOCK`, which is a map of blocks and not a naming rule: a
/// chiseled copper and a copper golem statue weather through their own chains.
namespace weathering {

[[nodiscard]] std::optional<std::string_view> next(std::string_view block);

/// What this block weathered from, which is what an axe scrapes it back to. parity:
/// `WeatheringCopper.getPrevious`, the inverse of the same map.
[[nodiscard]] std::optional<std::string_view> previous(std::string_view block);

/// How weathered a block already is: 0 for fresh copper, 3 for oxidized.
[[nodiscard]] std::optional<std::int32_t> stageOf(std::string_view block);

[[nodiscard]] bool weathers(std::string_view block);

/// Whether a block entity survives the block turning from one of these into the other.
///
/// parity: `Block.shouldChangedStateKeepBlockEntity`, which only the copper chest and the copper
/// golem statue override, and both answer the same way: yes while the old block is in their own tag.
/// It is what keeps a copper chest's contents through weathering, waxing and a scrape.
[[nodiscard]] bool keepsBlockEntity(std::string_view from, std::string_view to);

} // namespace weathering

} // namespace basalt::server::registry
