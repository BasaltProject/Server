#pragma once

#include <basalt/server/world/loot/LootPredicates.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::loot {

struct ItemStack {
    wire::common::Identifier item;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;

    friend bool operator==(const ItemStack&, const ItemStack&) = default;
};

struct LootTable;

/// parity: `Enchantment.Cost`, linear in the level.
struct EnchantmentCost {
    std::int32_t base = 0;
    std::int32_t perLevelAboveFirst = 0;

    [[nodiscard]] std::int32_t at(std::int32_t level) const { return base + (perLevelAboveFirst * (level - 1)); }
};

/// What an enchanting function reads off one enchantment. parity: `Enchantment.EnchantmentDefinition`.
struct EnchantmentEntry {
    wire::common::Identifier id;
    std::int32_t weight = 1;
    std::int32_t maxLevel = 1;
    EnchantmentCost minCost;
    EnchantmentCost maxCost;
};

struct Enchanting {
    std::function<std::vector<const EnchantmentEntry*>(const std::vector<std::string>& options)> candidates;
    /// parity: `Enchantment.canEnchant`, the `supported_items` set.
    std::function<bool(const wire::common::Identifier& enchantment, const wire::common::Identifier& item)> supports;
    /// parity: `Enchantment.isPrimaryItem`.
    std::function<bool(const wire::common::Identifier& enchantment, const wire::common::Identifier& item)> primary;
    /// parity: `Enchantment.areCompatible`.
    std::function<bool(const wire::common::Identifier& first, const wire::common::Identifier& second)> compatible;
    /// parity: the `enchantable` component, or nothing for an item that takes no enchanting.
    std::function<std::optional<std::int32_t>(const wire::common::Identifier& item)> enchantability;
};

/// Which entity of a roll a condition or a score asks about. parity: `LootContext.EntityTarget`.
enum class EntityTarget : std::uint8_t {
    This,
    Attacker,
    DirectAttacker,
    AttackingPlayer,
    TargetEntity,
    InteractingEntity
};

/// What an explorer map function asks the world for. parity: `ExplorationMapFunction`'s fields.
struct ExplorationMapRequest {
    std::vector<std::string> destination;
    std::string decoration = "minecraft:mansion";
    std::int8_t zoom = 2;
    std::int32_t searchRadius = 50;
    bool skipExistingChunks = true;
};

/// What a roll knows about where it came from. parity: `LootContextParams`.
struct LootContext {
    /// parity: `BLOCK_STATE`, the properties of the block behind the roll.
    std::map<std::string, std::string> blockState;
    std::optional<wire::common::Identifier> block;
    /// parity: `LootContextParams.THIS_ENTITY`.
    bool hasEntity = false;
    /// parity: `LootParams.withDynamicDrop`.
    std::map<std::string, std::vector<ItemStack>> dynamic;
    /// The tables a `loot_table` entry names. parity: `LootContext.getResolver`.
    std::function<const LootTable*(const wire::common::Identifier& id)> tables;
    /// parity: `FishingHookPredicate.inOpenWater`.
    std::optional<bool> inOpenWater;
    /// parity: `ADDITIONAL_COST_COMPONENT_ALLOWED`.
    bool additionalCostAllowed = false;
    /// parity: `LootContextParams.TOOL`, what broke the block, and its enchantments by id.
    std::optional<wire::common::Identifier> tool;
    std::map<std::string, std::int32_t, std::less<>> toolEnchantments;
    std::optional<wire::nbt::NbtCompound> toolComponents;
    /// parity: `LAST_DAMAGE_PLAYER`, whether a player's hit is behind the death.
    bool killedByPlayer = false;
    /// parity: `THIS_ENTITY`.
    std::optional<EntityFacts> thisEntity;
    std::optional<EntityFacts> attacker;
    std::optional<EntityFacts> directAttacker;
    /// parity: `LAST_DAMAGE_PLAYER`.
    std::optional<EntityFacts> attackingPlayer;
    /// parity: `EnchantmentHelper.getEnchantmentLevel(enchantment, attacker)`.
    std::map<std::string, std::int32_t, std::less<>> attackerEnchantments;
    TagLookup tags;
    /// parity: `RecipeType.SMELTING`, what a furnace makes of an item, for `furnace_smelt`.
    std::function<std::optional<ItemStack>(const ItemStack& input)> smelt;
    const Enchanting* enchanting = nullptr;
    std::function<std::vector<wire::common::Identifier>(std::string_view registry, const wire::common::Identifier& tag)>
        tagMembers;
    /// parity: `LootParams.getLuck`, what `fill` rolls with: the opening player's luck.
    float luck = 0.0F;
    /// parity: `LootContextParams.ORIGIN`, and what a `location_check` sees at a square.
    std::optional<std::array<double, 3>> origin;
    std::function<PlaceFacts(std::int32_t x, std::int32_t y, std::int32_t z)> placeAt;
    /// parity: the damage type's tags, for `damage_source_properties`.
    std::function<bool(std::string_view tag)> damageTagged;
    std::optional<wire::common::Identifier> damageType;
    /// parity: `EXPLOSION_RADIUS`, what `survives_explosion` and `explosion_decay` read.
    std::optional<float> explosionRadius;
    /// parity: `ENCHANTMENT_LEVEL` and `ENCHANTMENT_ACTIVE`.
    std::optional<std::int32_t> enchantmentLevel;
    std::optional<bool> enchantmentActive;
    /// parity: `Scoreboard.getPlayerScoreInfo`.
    std::function<std::optional<std::int32_t>(std::string_view holder, std::string_view objective)> score;
    /// The score holder name of one of the roll's entities. parity: `Entity.getScoreboardName`.
    std::function<std::optional<std::string>(EntityTarget target)> scoreHolder;
    /// parity: `StoredNumberAccess`, the one number at `path` in command storage `id`.
    std::function<std::optional<double>(const wire::common::Identifier& id, std::string_view path)> storage;
    /// parity: `EnvironmentAttributeSystem.getValue` at the roll's origin, as a number.
    std::function<std::optional<double>(const wire::common::Identifier& attribute)> environment;
    /// parity: `ClockManager`'s total ticks for a world clock.
    std::function<std::optional<std::int64_t>(const wire::common::Identifier& clock)> clockTicks;
    /// parity: `WeatherCheck`, whether it rains and whether it thunders.
    std::optional<bool> raining;
    std::optional<bool> thundering;
    /// parity: `ExplorationMapFunction.run`.
    std::function<void(ItemStack& stack, const ExplorationMapRequest& request)> explorationMap;
    /// parity: `SlotProvider.getSlotsFromRange`.
    std::function<std::vector<ItemStack>(std::string_view source, std::string_view slots)> slotsOf;
    std::function<std::vector<wire::common::Identifier>(std::string_view registry)> registryEntries;
    /// parity: `ComponentUtils.updateForEntity`, a text component with its selectors and scores filled in for `holder`.
    std::function<std::optional<wire::nbt::NbtTag>(const wire::nbt::NbtTag& component, std::string_view holder)>
        resolveText;
};

/// The holder name of an entity target. parity:
[[nodiscard]] std::optional<std::string> scoreHolderOf(const LootContext& context, EntityTarget target);

/// parity: `LevelBasedValue`, the two shapes the loot tables use.
struct LevelBasedValue {
    float base = 0.0F;
    float perLevelAboveFirst = 0.0F;

    [[nodiscard]] float at(std::int32_t level) const {
        return base + (perLevelAboveFirst * static_cast<float>(level - 1));
    }
};

} // namespace basalt::server::world::loot
