#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/ContextNumbers.hpp>
#include <basalt/server/world/loot/LootCondition.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot {

struct ComponentEdit;
struct SlotSource;

/// A post-processing step applied to a generated stack. parity: `LootItemFunction`, every one of
class LootFunction {
public:
    enum class Kind : std::uint8_t {
        EnchantedCountIncrease,
        ExplosionDecay,
        CopyFromBlock,
        ApplyBonus,
        /// parity: `LimitCount`, the clamp `IntLimit` puts on whatever came before it.
        LimitCount,
        SetCount,
        /// parity: `SmeltItemFunction`, the furnace's result in place of the item.
        Smelt,
        EnchantRandomly,
        EnchantWithLevels,
        SetEnchantments,
        /// parity: `SetItemDamageFunction`, a fraction of the durability left.
        SetDamage,
        /// parity: `SetComponentsFunction`, `SetNameFunction` and `SetPotionFunction`: components.
        SetComponents,
        SetStewEffect,
        SetInstrument,
        /// parity: `SequenceFunction`, the functions in order.
        Sequence,
        /// parity: `FilteredFunction`, one modifier or the other by an item predicate.
        Filtered,
        /// parity: `DiscardItem`, which leaves nothing.
        Discard,
        SetOminousBottleAmplifier,
        SetRandomPotion,
        SetRandomDyes,
        /// parity: `ExplorationMapFunction`, answered by the context.
        ExplorationMap,
        EditComponents,
    };

    [[nodiscard]] static LootFunction enchantedCountIncrease(FloatProvider count,
                                                             std::optional<std::int32_t> limit = std::nullopt);
    [[nodiscard]] static LootFunction explosionDecay();
    [[nodiscard]] static LootFunction copyFromBlock();
    [[nodiscard]] static LootFunction smelt(bool useInputCount = true);
    [[nodiscard]] static LootFunction setCount(IntProvider count, bool add = false);

    /// parity: `ApplyBonusCount`'s three formulas.
    enum class BonusFormula : std::uint8_t { OreDrops, UniformBonusCount, BinomialWithBonusCount };
    [[nodiscard]] static LootFunction applyBonus(std::string enchantment, BonusFormula formula,
                                                 std::int32_t multiplierOrExtra, float probability);
    [[nodiscard]] static LootFunction enchantedCountIncrease(std::string enchantment, FloatProvider count,
                                                             std::optional<std::int32_t> limit);
    [[nodiscard]] static LootFunction limitCount(std::optional<IntProvider> least, std::optional<IntProvider> most);
    [[nodiscard]] static LootFunction enchantRandomly(std::vector<std::string> options, bool onlyCompatible);
    [[nodiscard]] static LootFunction enchantWithLevels(IntProvider levels, std::vector<std::string> options);
    [[nodiscard]] static LootFunction setEnchantments(std::vector<std::pair<std::string, IntProvider>> levels,
                                                      bool add);
    [[nodiscard]] static LootFunction setDamage(FloatProvider damage, bool add);
    [[nodiscard]] static LootFunction setComponents(wire::nbt::NbtCompound components,
                                                    std::vector<std::string> removed = {},
                                                    std::optional<EntityTarget> resolveFor = std::nullopt,
                                                    std::vector<std::string> resolveKeys = {});
    [[nodiscard]] static LootFunction setStewEffect(std::vector<std::pair<std::string, IntProvider>> effects);
    [[nodiscard]] static LootFunction setInstrument(std::vector<std::string> options);
    [[nodiscard]] static LootFunction sequence(std::vector<LootFunction> functions);
    [[nodiscard]] static LootFunction filtered(ItemPredicate filter, std::optional<LootFunction> onPass,
                                               std::optional<LootFunction> onFail);
    [[nodiscard]] static LootFunction discard();
    [[nodiscard]] static LootFunction setOminousBottleAmplifier(IntProvider amplifier);
    [[nodiscard]] static LootFunction setRandomPotion(std::vector<std::string> options, bool everyPotion = false);
    [[nodiscard]] static LootFunction setRandomDyes(IntProvider count);
    [[nodiscard]] static LootFunction explorationMap(ExplorationMapRequest request);
    [[nodiscard]] static LootFunction editComponents(std::shared_ptr<const ComponentEdit> edit);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    /// parity: `include_additional_cost_component` on the two enchanting functions.
    void includeAdditionalCost(bool include) noexcept { additionalCost_ = include; }

    /// parity: `LootItemConditionalFunction.condition`.
    std::vector<LootCondition> conditions;

    [[nodiscard]] ItemStack apply(ItemStack stack, random::RandomSource& random,
                                  std::optional<float> explosionRadius = std::nullopt,
                                  const LootContext& context = {}) const;

private:
    struct Parts;

    Kind kind_ = Kind::SetCount;
    std::optional<std::int32_t> limit_;
    bool add_ = false;
    BonusFormula formula_ = BonusFormula::OreDrops;
    std::int32_t bonusNumber_ = 0;
    float probability_ = 0.0F;
    bool onlyCompatible_ = true;
    bool additionalCost_ = false;
    std::shared_ptr<const Parts> parts_;
    std::shared_ptr<const ComponentEdit> edit_;

    [[nodiscard]] const Parts& parts() const;
    [[nodiscard]] ItemStack run(ItemStack stack, random::RandomSource& random, std::optional<float> explosionRadius,
                                const LootContext& context) const;
    [[nodiscard]] ItemStack enchant(ItemStack stack, random::RandomSource& random, const LootContext& context) const;
    [[nodiscard]] ItemStack setComponent(ItemStack stack, random::RandomSource& random,
                                         const LootContext& context) const;
};

struct NestedTable {
    std::optional<wire::common::Identifier> id;
    std::shared_ptr<const LootTable> inlined;
};

/// One entry of a pool. parity: `LootPoolEntryContainer` and its nine types: the leaves `item`.
struct LootEntry {
    enum class Composite : std::uint8_t { None, Alternatives, Sequence, Group };

    std::optional<wire::common::Identifier> item;
    /// parity: `DynamicLoot`.
    std::optional<std::string> dynamicName;
    /// parity: `NestedLootTable`.
    std::vector<NestedTable> tables;
    std::optional<wire::common::Identifier> tableTag;
    /// parity: `TagEntry`, the items of an item holder set: a `#tag`, or ids written out.
    std::optional<wire::common::Identifier> tag;
    std::vector<wire::common::Identifier> tagItems;
    bool expand = false;
    /// parity: `EmptyLootItem`, a choice that makes nothing.
    bool empty = false;
    /// parity: `SlotLoot`.
    std::shared_ptr<const SlotSource> slotSource;
    std::int32_t weight = 1;
    std::int32_t quality = 0;
    /// parity: `LootPoolEntryContainer.condition`, gating the whole entry.
    std::vector<LootCondition> conditions;
    /// parity: `LootPoolEntryContainer.modifier`, applied to every stack the entry hands on.
    std::vector<LootFunction> functions;
    Composite composite = Composite::None;
    std::vector<LootEntry> children;

    /// parity: `EntryBase`, `max(floor(weight + quality * luck), 0)`.
    [[nodiscard]] std::int32_t weightFor(float luck) const;

    [[nodiscard]] bool canRun(random::RandomSource& random, const LootContext& context = {}) const;
};

/// One pool: rolls a number of times, picking one weighted-random entry per roll. parity: `LootPool`
struct LootPool {
    std::vector<LootEntry> entries;
    IntProvider rolls = IntProvider::constant(1);
    FloatProvider bonusRolls = FloatProvider::constant(0.0F);
    std::vector<LootCondition> conditions;
    std::vector<LootFunction> functions;

    using Output = std::function<void(ItemStack)>;

    void generate(random::RandomSource& random, float luck, std::optional<float> explosionRadius, const Output& output,
                  const LootContext& context = {}) const;
};

/// moment it is produced. parity: `LootTable.getRandomItemsRaw`, which decorates the per-pool output
struct LootTable {
    std::vector<LootPool> pools;
    std::vector<LootFunction> functions;
    /// The named stream this table rolls on. parity: `LootTable.randomSequence`, which is what makes
    std::optional<wire::common::Identifier> randomSequence;

    /// parity: `LootTable.getRandomItemsRaw`, which is also what a nested entry runs.
    void generateRaw(random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                     const LootPool::Output& output, const LootContext& context = {}) const;

    /// parity: `LootTable.fill`.
    void fill(std::vector<std::optional<ItemStack>>& slots, random::RandomSource& random,
              const LootContext& context = {}) const;

    /// parity: `LootTable.getRandomItems`.
    [[nodiscard]] std::vector<ItemStack> generate(random::RandomSource& random, float luck = 0.0F,
                                                  std::optional<float> explosionRadius = std::nullopt,
                                                  const LootContext& context = {}) const;
};

} // namespace basalt::server::world::loot
