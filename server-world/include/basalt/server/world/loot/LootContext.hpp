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

/// A stack of an item: what a loot table produces and what a crafting grid holds.
///
/// `components` is what the item carries beyond its own defaults, in the same form the rest of the
/// server stores it. Most of what comes out of a loot table has none; a crafting recipe that dyes,
/// repairs, copies a book or decorates a shield is defined entirely by what it does to them, so the
/// grid has to carry them or those recipes cannot exist at all.
struct ItemStack {
    wire::common::Identifier item;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;
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

/// The enchantment registry as the loot functions ask it. The data is the operator's jar, so the
/// caller supplies it.
struct Enchanting {
    /// In registry order: all of them for no options, else what the ids or the one `#tag` name.
    std::function<std::vector<const EnchantmentEntry*>(const std::vector<std::string>& options)> candidates;
    /// parity: `Enchantment.canEnchant`, the `supported_items` set.
    std::function<bool(const wire::common::Identifier& enchantment, const wire::common::Identifier& item)> supports;
    /// parity: `Enchantment.isPrimaryItem`, the `primary_items` set or else the supported one.
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
    /// Structure ids, or one `#tag`.
    std::vector<std::string> destination;
    std::string decoration = "minecraft:mansion";
    std::int8_t zoom = 2;
    std::int32_t searchRadius = 50;
    bool skipExistingChunks = true;
};

/// What a roll knows about where it came from. parity: `LootContextParams`, the parts this server
/// can answer; anything a caller leaves empty is a parameter the roll does not have.
struct LootContext {
    /// parity: `BLOCK_STATE`, the properties of the block behind the roll.
    std::map<std::string, std::string> blockState;
    /// The block `blockState` belongs to. A caller that leaves it empty is trusted to roll a block's
    /// own table, so `match_block` then checks the state alone.
    std::optional<wire::common::Identifier> block;
    /// Whether somebody is behind the break: the player mining it or the projectile that hit it.
    /// parity: `LootContextParams.THIS_ENTITY`, which a block broken by a neighbour or a blast has
    /// none of. Snow and the chorus flower are the two tables that ask.
    bool hasEntity = false;
    /// parity: `LootParams.withDynamicDrop`, the drops a block hands the roll itself rather than
    /// naming in its table. The only one in the game is a decorated pot's four sherds.
    std::map<std::string, std::vector<ItemStack>> dynamic;
    /// The tables a `loot_table` entry names. parity: `LootContext.getResolver`.
    std::function<const LootTable*(const wire::common::Identifier& id)> tables;
    /// Whether the fishing hook behind the roll is in open water, or nothing without a hook.
    /// parity: `FishingHookPredicate.inOpenWater`.
    std::optional<bool> inOpenWater;
    /// parity: `ADDITIONAL_COST_COMPONENT_ALLOWED`, set only when a villager works out its offers.
    bool additionalCostAllowed = false;
    /// parity: `LootContextParams.TOOL`, what broke the block, and its enchantments by id.
    std::optional<wire::common::Identifier> tool;
    std::map<std::string, std::int32_t, std::less<>> toolEnchantments;
    /// The tool's whole component patch, when the caller has it: `match_tool` then reads every part
    /// of its predicate rather than the item and enchantments alone.
    std::optional<wire::nbt::NbtCompound> toolComponents;
    /// parity: `LAST_DAMAGE_PLAYER`, whether a player's hit is behind the death.
    bool killedByPlayer = false;
    /// parity: `THIS_ENTITY`, `ATTACKING_ENTITY` and `DIRECT_ATTACKING_ENTITY` as a predicate sees them.
    std::optional<EntityFacts> thisEntity;
    std::optional<EntityFacts> attacker;
    std::optional<EntityFacts> directAttacker;
    /// parity: `LAST_DAMAGE_PLAYER`, the player behind the death whatever landed the last blow.
    std::optional<EntityFacts> attackingPlayer;
    /// The highest level of each enchantment across what the attacker wears and holds.
    /// parity: `EnchantmentHelper.getEnchantmentLevel(enchantment, attacker)`.
    std::map<std::string, std::int32_t, std::less<>> attackerEnchantments;
    TagLookup tags;
    /// parity: `RecipeType.SMELTING`, what a furnace makes of an item, for `furnace_smelt`.
    std::function<std::optional<ItemStack>(const ItemStack& input)> smelt;
    /// The enchantment registry, for the enchanting functions; without it they leave the item alone.
    const Enchanting* enchanting = nullptr;
    /// A tag's members in the order the tag lists them, for a `tag` entry and `set_instrument`.
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
    /// parity: `ENCHANTMENT_LEVEL` and `ENCHANTMENT_ACTIVE`, set only by an enchantment effect.
    std::optional<std::int32_t> enchantmentLevel;
    std::optional<bool> enchantmentActive;
    /// parity: `Scoreboard.getPlayerScoreInfo`, the score of the named holder, or nothing without one.
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
    /// parity: `ExplorationMapFunction.run`, which finds the structure and draws the map. Unset, the
    /// stack is left as it is, which a `map_id` filter after it then throws away.
    std::function<void(ItemStack& stack, const ExplorationMapRequest& request)> explorationMap;
};

/// The score holder behind one of the roll's entities: the context's own answer when it has one,
/// else the name its facts carry. parity: `LootContext.getOptionalParameter(...).getScoreboardName`.
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
