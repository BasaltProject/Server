#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

/// A value that may be fixed or randomly rolled when a loot table asks for it: roll counts, item
/// counts, condition chances. parity: `NumberProvider`. Only the two real vanilla loot tables use
/// almost everywhere are implemented, `constant` and `uniform`; the rest are a documented gap.
class NumberProvider {
public:
    enum class Kind : std::uint8_t { Constant, Uniform, Binomial };

    [[nodiscard]] static NumberProvider constant(float value);
    [[nodiscard]] static NumberProvider uniform(NumberProvider min, NumberProvider max);
    /// parity: `BinomialDistributionGenerator`, `n` tries at chance `p`.
    [[nodiscard]] static NumberProvider binomial(NumberProvider n, NumberProvider p);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    /// parity: `Mth.nextInt`, inclusive at both ends and short-circuiting when they meet.
    [[nodiscard]] std::int32_t getInt(random::RandomSource& random) const;

    /// parity: `Mth.nextFloat`, `min + nextFloat() * (max - min)`.
    [[nodiscard]] float getFloat(random::RandomSource& random) const;

private:
    Kind kind_ = Kind::Constant;
    float value_ = 0.0F;
    std::shared_ptr<const NumberProvider> min_;
    std::shared_ptr<const NumberProvider> max_;
};

/// What a roll knows about where it came from.
///
/// parity: `LootContextParams`. The only one this evaluator reads is `BLOCK_STATE`, which is what
/// tells the lower half of a door from the upper; a roll with no block behind it leaves it empty and
/// every state predicate answers no.
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

struct LootContext {
    std::map<std::string, std::string> blockState;
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
    /// parity: `LootContextParams.TOOL`, what broke the block, and its enchantments by id.
    std::optional<wire::common::Identifier> tool;
    std::map<std::string, std::int32_t, std::less<>> toolEnchantments;
    /// parity: `LAST_DAMAGE_PLAYER`, whether a player's hit is behind the death.
    bool killedByPlayer = false;
    /// parity: `THIS_ENTITY`, `ATTACKING_ENTITY` and `DIRECT_ATTACKING_ENTITY` as a predicate sees them.
    std::optional<EntityFacts> thisEntity;
    std::optional<EntityFacts> attacker;
    std::optional<EntityFacts> directAttacker;
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
    /// parity: `LootContextParams.ORIGIN`, and what a `location_check` sees at a square.
    std::optional<std::array<double, 3>> origin;
    std::function<PlaceFacts(std::int32_t x, std::int32_t y, std::int32_t z)> placeAt;
    /// parity: the damage type's tags, for `damage_source_properties`.
    std::function<bool(std::string_view tag)> damageTagged;
};

/// parity: `LevelBasedValue`, the two shapes the loot tables use.
struct LevelBasedValue {
    float base = 0.0F;
    float perLevelAboveFirst = 0.0F;

    [[nodiscard]] float at(std::int32_t level) const {
        return base + (perLevelAboveFirst * static_cast<float>(level - 1));
    }
};

/// A predicate gating a pool or entry. parity: `LootItemCondition`. The rest need an entity context
/// this evaluator does not model yet, and are a documented gap.
class LootCondition {
public:
    enum class Kind : std::uint8_t {
        RandomChance,
        /// Always true: nothing this build breaks is broken by an explosion.
        SurvivesExplosion,
        /// Always false: which tool broke the block never reaches this evaluator.
        MatchTool,
        /// Always false: a mob killed here is not on fire and not a baby.
        EntityProperties,
        /// Whether there is a `THIS_ENTITY` at all, which is what an empty predicate asks.
        ThisEntityExists,
        /// Always true: everything that dies here dies to a player or to a command.
        KilledByPlayer,
        Inverted,
        AnyOf,
        AllOf,
        /// The plain chance, since nothing carries an enchantment into a roll here.
        RandomChanceWithEnchantedBonus,
        /// The state of the block that broke, which is what a door or a slab drops by.
        BlockStateProperty,
        /// Always true: what is around the block that broke does not reach this evaluator.
        LocationCheck,
        /// parity: `entity_properties` with `type_specific/fishing_hook`, the only one it models.
        FishingHookOpenWater,
        /// parity: `entity_properties` with a predicate this build models, on one of three entities.
        EntityMatches,
        /// parity: `BonusLevelTableCondition`, a chance per level of the tool's enchantment.
        TableBonus,
        /// parity: `DamageSourceCondition`: the damage's tags and who dealt it.
        DamageSource,
    };

    enum class Target : std::uint8_t { This, Attacker, DirectAttacker };

    [[nodiscard]] static LootCondition randomChance(NumberProvider chance);
    [[nodiscard]] static LootCondition survivesExplosion();
    [[nodiscard]] static LootCondition matchTool();
    [[nodiscard]] static LootCondition entityProperties();
    [[nodiscard]] static LootCondition thisEntityExists();
    [[nodiscard]] static LootCondition killedByPlayer();
    [[nodiscard]] static LootCondition inverted(LootCondition inner);
    [[nodiscard]] static LootCondition anyOf(std::vector<LootCondition> terms);
    [[nodiscard]] static LootCondition allOf(std::vector<LootCondition> terms);
    [[nodiscard]] static LootCondition randomChanceWithEnchantedBonus(float chance);
    [[nodiscard]] static LootCondition blockStateProperty(std::map<std::string, std::string> expected);
    [[nodiscard]] static LootCondition locationCheck();
    /// parity: `LocationCheck`, the predicate at the origin moved by the offsets.
    [[nodiscard]] static LootCondition locationCheck(LocationPredicate predicate, std::array<double, 3> offset);
    [[nodiscard]] static LootCondition fishingHookOpenWater(bool expected);
    [[nodiscard]] static LootCondition matchTool(ItemPredicate predicate);
    [[nodiscard]] static LootCondition entityMatches(Target target, EntityPredicate predicate);
    [[nodiscard]] static LootCondition tableBonus(std::string enchantment, std::vector<float> chances);
    /// `tags` pairs a damage type tag with whether the damage must carry it.
    [[nodiscard]] static LootCondition damageSource(std::vector<std::pair<std::string, bool>> tags,
                                                    std::optional<EntityPredicate> source,
                                                    std::optional<EntityPredicate> direct);
    [[nodiscard]] static LootCondition randomChanceWithEnchantedBonus(float unenchanted, LevelBasedValue enchanted,
                                                                      std::string enchantment);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    [[nodiscard]] bool test(random::RandomSource& random, const LootContext& context = {}) const;

private:
    Kind kind_ = Kind::SurvivesExplosion;
    NumberProvider chance_;
    float plainChance_ = 0.0F;
    std::shared_ptr<const LootCondition> inner_;
    std::vector<LootCondition> terms_;
    std::map<std::string, std::string> expected_;
    std::shared_ptr<const ItemPredicate> item_;
    std::shared_ptr<const EntityPredicate> entity_;
    Target target_ = Target::This;
    std::string enchantment_;
    std::vector<float> chances_;
    LevelBasedValue enchanted_;
    std::vector<std::pair<std::string, bool>> damageTags_;
    std::shared_ptr<const EntityPredicate> direct_;
    std::shared_ptr<const LocationPredicate> location_;
    std::array<double, 3> offset_{};
};

/// A post-processing step applied to a generated stack. parity: `LootItemFunction`.
class LootFunction {
public:
    enum class Kind : std::uint8_t {
        /// The looting bonus every mob's table carries; the level is zero here, so it adds nothing.
        EnchantedCountIncrease,
        /// What an explosion swallows, one item at a time.
        ExplosionDecay,
        /// Something copied off the block entity; the caller that knows the block puts it on.
        CopyFromBlock,
        /// The fortune bonus, which adds nothing here: no tool reaches this evaluator, so the level
        /// is always zero and every formula `ApplyBonusCount` has answers the count it was given.
        ApplyBonus,
        /// The clamp `LimitCount` puts on whatever came before it.
        LimitCount,
        /// The cooked drop a mob killed by fire leaves; nothing here dies on fire yet.
        FurnaceSmelt,
        SetCount,
        /// parity: `SmeltItemFunction`, the furnace's result in place of the item.
        Smelt,
        /// parity: `EnchantRandomlyFunction`.
        EnchantRandomly,
        /// parity: `EnchantWithLevelsFunction`.
        EnchantWithLevels,
        /// parity: `SetEnchantmentsFunction`.
        SetEnchantments,
        /// parity: `SetItemDamageFunction`, a fraction of the durability left.
        SetDamage,
        /// parity: `SetComponentsFunction` and `SetNameFunction`: components written as given.
        SetComponents,
        /// parity: `SetStewEffectFunction`.
        SetStewEffect,
        /// parity: `SetInstrumentFunction`.
        SetInstrument,
    };

    [[nodiscard]] static LootFunction enchantedCountIncrease(NumberProvider count,
                                                             std::optional<std::int32_t> limit = std::nullopt);
    [[nodiscard]] static LootFunction explosionDecay();
    [[nodiscard]] static LootFunction copyFromBlock();
    [[nodiscard]] static LootFunction furnaceSmelt();
    [[nodiscard]] static LootFunction smelt();
    [[nodiscard]] static LootFunction setCount(NumberProvider count, bool add = false);
    [[nodiscard]] static LootFunction applyBonus();

    /// parity: `ApplyBonusCount`'s three formulas.
    enum class BonusFormula : std::uint8_t { OreDrops, UniformBonusCount, BinomialWithBonusCount };
    [[nodiscard]] static LootFunction applyBonus(std::string enchantment, BonusFormula formula,
                                                 std::int32_t multiplierOrExtra, float probability);
    [[nodiscard]] static LootFunction enchantedCountIncrease(std::string enchantment, NumberProvider count,
                                                             std::optional<std::int32_t> limit);
    [[nodiscard]] static LootFunction limitCount(std::optional<std::int32_t> least, std::optional<std::int32_t> most);
    [[nodiscard]] static LootFunction enchantRandomly(std::vector<std::string> options, bool onlyCompatible);
    [[nodiscard]] static LootFunction enchantWithLevels(NumberProvider levels, std::vector<std::string> options);
    [[nodiscard]] static LootFunction setEnchantments(std::vector<std::pair<std::string, NumberProvider>> levels,
                                                      bool add);
    [[nodiscard]] static LootFunction setDamage(NumberProvider damage, bool add);
    /// `removed` names the components a `!` prefix takes off.
    [[nodiscard]] static LootFunction setComponents(wire::nbt::NbtCompound components,
                                                    std::vector<std::string> removed = {});
    [[nodiscard]] static LootFunction setStewEffect(std::vector<std::pair<std::string, NumberProvider>> effects);
    [[nodiscard]] static LootFunction setInstrument(std::string options);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    /// parity: `LootItemConditionalFunction`, which every function in a block table is: a set_count
    /// that only applies to one state of the block is how a candle drops as many as it had.
    std::vector<LootCondition> conditions;

    [[nodiscard]] ItemStack apply(ItemStack stack, random::RandomSource& random,
                                  std::optional<float> explosionRadius = std::nullopt,
                                  const LootContext& context = {}) const;

private:
    Kind kind_ = Kind::SetCount;
    NumberProvider count_;
    std::optional<std::int32_t> limit_;
    std::optional<std::int32_t> least_;
    bool add_ = false;
    std::string enchantment_;
    BonusFormula formula_ = BonusFormula::OreDrops;
    std::int32_t bonusNumber_ = 0;
    float probability_ = 0.0F;
    std::vector<std::string> options_;
    bool onlyCompatible_ = true;
    std::vector<std::pair<std::string, NumberProvider>> levels_;
    std::shared_ptr<const wire::nbt::NbtCompound> components_;
    std::vector<std::string> removed_;

    [[nodiscard]] ItemStack enchant(ItemStack stack, random::RandomSource& random, const LootContext& context) const;
    [[nodiscard]] ItemStack setComponent(ItemStack stack, random::RandomSource& random,
                                         const LootContext& context) const;
};

/// A single entry in a pool: either an `item` leaf or an `alternatives` composite whose children are
/// tried in order. parity: `LootItem` and `AlternativesEntry`; the other composites are a gap.
struct LootEntry {
    std::optional<wire::common::Identifier> item;
    /// parity: `DynamicLoot`, an entry whose stacks come from the block rather than from the table.
    std::optional<std::string> dynamicName;
    /// parity: `NestedLootTable`, an entry whose stacks are another table's roll.
    std::optional<wire::common::Identifier> table;
    /// The same, written in place rather than named.
    std::shared_ptr<const LootTable> inlineTable;
    /// parity: `TagEntry`, every item of an item tag: one stack each, or one choice each when expanded.
    std::optional<wire::common::Identifier> tag;
    bool expand = false;
    /// parity: `EmptyLootItem`, a choice that makes nothing.
    bool empty = false;
    std::int32_t weight = 1;
    std::int32_t quality = 0;
    std::vector<LootCondition> conditions;
    std::vector<LootFunction> functions;
    std::vector<LootEntry> alternatives;

    /// parity: `EntryBase`, `max(floor(weight + quality * luck), 0)`.
    [[nodiscard]] std::int32_t weightFor(float luck) const;

    [[nodiscard]] bool canRun(random::RandomSource& random, const LootContext& context = {}) const;

    [[nodiscard]] ItemStack createItemStack(random::RandomSource& random, const LootContext& context = {}) const;
};

/// One pool: rolls a number of times, picking one weighted-random entry per roll. parity: `LootPool`
/// and its `addRandomItems`. Entry conditions are re-evaluated on every single roll, and the pick is
/// `nextInt(totalWeight)` then a linear scan subtracting weights until negative.
struct LootPool {
    std::vector<LootEntry> entries;
    NumberProvider rolls = NumberProvider::constant(1.0F);
    NumberProvider bonusRolls = NumberProvider::constant(0.0F);
    std::vector<LootCondition> conditions;
    std::vector<LootFunction> functions;

    using Output = std::function<void(ItemStack)>;

    void generate(random::RandomSource& random, float luck, std::optional<float> explosionRadius, const Output& output,
                  const LootContext& context = {}) const;
};

/// A full loot table: every pool rolled in order, with table-level functions applied to each item the
/// moment it is produced. parity: `LootTable.getRandomItemsRaw`, which decorates the per-pool output
/// consumer with the table's composite function, so random-consuming table functions interleave with
/// pool rolls in the same order a real server would see.
struct LootTable {
    std::vector<LootPool> pools;
    std::vector<LootFunction> functions;
    /// The named stream this table rolls on. parity: `LootTable.randomSequence`, which is what makes
    /// the same world drop the same thing in the same order.
    std::optional<wire::common::Identifier> randomSequence;

    /// Every stack the pools make, with the table's functions applied and nothing filtered.
    /// parity: `LootTable.getRandomItemsRaw`, which is also what a nested entry runs.
    void generateRaw(random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                     const LootPool::Output& output, const LootContext& context = {}) const;

    /// parity: `LootTable.fill`: the roll spread over the empty slots, stacks split to cover them.
    void fill(std::vector<std::optional<ItemStack>>& slots, random::RandomSource& random,
              const LootContext& context = {}) const;

    [[nodiscard]] std::vector<ItemStack> generate(random::RandomSource& random, float luck = 0.0F,
                                                  std::optional<float> explosionRadius = std::nullopt,
                                                  const LootContext& context = {}) const;
};

} // namespace basalt::server::world::loot
