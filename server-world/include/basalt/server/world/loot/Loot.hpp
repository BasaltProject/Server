#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
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
    enum class Kind : std::uint8_t { Constant, Uniform };

    [[nodiscard]] static NumberProvider constant(float value);
    [[nodiscard]] static NumberProvider uniform(NumberProvider min, NumberProvider max);

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
struct LootContext {
    std::map<std::string, std::string> blockState;
    /// parity: `LootParams.withDynamicDrop`, the drops a block hands the roll itself rather than
    /// naming in its table. The only one in the game is a decorated pot's four sherds.
    std::map<std::string, std::vector<ItemStack>> dynamic;
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
    };

    [[nodiscard]] static LootCondition randomChance(NumberProvider chance);
    [[nodiscard]] static LootCondition survivesExplosion();
    [[nodiscard]] static LootCondition matchTool();
    [[nodiscard]] static LootCondition entityProperties();
    [[nodiscard]] static LootCondition killedByPlayer();
    [[nodiscard]] static LootCondition inverted(LootCondition inner);
    [[nodiscard]] static LootCondition anyOf(std::vector<LootCondition> terms);
    [[nodiscard]] static LootCondition allOf(std::vector<LootCondition> terms);
    [[nodiscard]] static LootCondition randomChanceWithEnchantedBonus(float chance);
    [[nodiscard]] static LootCondition blockStateProperty(std::map<std::string, std::string> expected);
    [[nodiscard]] static LootCondition locationCheck();

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    [[nodiscard]] bool test(random::RandomSource& random, const LootContext& context = {}) const;

private:
    Kind kind_ = Kind::SurvivesExplosion;
    NumberProvider chance_;
    float plainChance_ = 0.0F;
    std::shared_ptr<const LootCondition> inner_;
    std::vector<LootCondition> terms_;
    std::map<std::string, std::string> expected_;
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
    };

    [[nodiscard]] static LootFunction enchantedCountIncrease(NumberProvider count,
                                                             std::optional<std::int32_t> limit = std::nullopt);
    [[nodiscard]] static LootFunction explosionDecay();
    [[nodiscard]] static LootFunction copyFromBlock();
    [[nodiscard]] static LootFunction furnaceSmelt();
    [[nodiscard]] static LootFunction setCount(NumberProvider count, bool add = false);
    [[nodiscard]] static LootFunction applyBonus();
    [[nodiscard]] static LootFunction limitCount(std::optional<std::int32_t> least, std::optional<std::int32_t> most);

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
};

/// A single entry in a pool: either an `item` leaf or an `alternatives` composite whose children are
/// tried in order. parity: `LootItem` and `AlternativesEntry`; the other composites are a gap.
struct LootEntry {
    std::optional<wire::common::Identifier> item;
    /// parity: `DynamicLoot`, an entry whose stacks come from the block rather than from the table.
    std::optional<std::string> dynamicName;
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

    [[nodiscard]] std::vector<ItemStack> generate(random::RandomSource& random, float luck = 0.0F,
                                                  std::optional<float> explosionRadius = std::nullopt,
                                                  const LootContext& context = {}) const;
};

} // namespace basalt::server::world::loot
