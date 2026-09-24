#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/ContextNumbers.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot {

/// parity: `IntRangePredicate`: one value to equal, or either end of a range, each a provider.
struct IntRange {
    std::optional<IntProvider> exact;
    std::optional<IntProvider> min;
    std::optional<IntProvider> max;

    [[nodiscard]] bool test(random::RandomSource& random, const LootContext& context, std::int32_t value) const;
};

/// parity: `FloatRangePredicate`, the same over fractions.
struct FloatRange {
    std::optional<FloatProvider> exact;
    std::optional<FloatProvider> min;
    std::optional<FloatProvider> max;

    [[nodiscard]] bool test(random::RandomSource& random, const LootContext& context, float value) const;
};

/// A predicate gating a pool, an entry, a function or a provider. parity: `LootItemCondition` and
/// the 20 types of 26.3.
class LootCondition {
public:
    enum class Kind : std::uint8_t {
        RandomChance,
        SurvivesExplosion,
        MatchTool,
        /// Always false: a predicate this build cannot read.
        EntityProperties,
        /// Whether there is a `THIS_ENTITY` at all, which is what an empty predicate asks.
        ThisEntityExists,
        KilledByPlayer,
        Inverted,
        AnyOf,
        AllOf,
        RandomChanceWithEnchantedBonus,
        /// parity: `MatchBlock`, the block behind the roll and its state.
        MatchBlock,
        LocationCheck,
        /// parity: `entity_properties` with `type_specific/fishing_hook`, the only one it models.
        FishingHookOpenWater,
        /// parity: `entity_properties` with a predicate this build models, on one of the entities.
        EntityMatches,
        /// parity: `BonusLevelTableCondition`, a chance per level of the tool's enchantment.
        TableBonus,
        /// parity: `DamageSourceCondition`: the damage's tags and who dealt it.
        DamageSource,
        /// Always true: the condition has no predicate at all.
        Always,
        IntValueCheck,
        FloatValueCheck,
        EnchantmentActiveCheck,
        TimeCheck,
        WeatherCheck,
        EntityScores,
        EnvironmentAttributeCheck,
    };

    enum class Target : std::uint8_t { This, Attacker, DirectAttacker, AttackingPlayer };

    [[nodiscard]] static LootCondition randomChance(FloatProvider chance);
    [[nodiscard]] static LootCondition always();
    [[nodiscard]] static LootCondition survivesExplosion();
    [[nodiscard]] static LootCondition matchTool();
    [[nodiscard]] static LootCondition entityProperties();
    [[nodiscard]] static LootCondition thisEntityExists();
    [[nodiscard]] static LootCondition killedByPlayer();
    [[nodiscard]] static LootCondition inverted(LootCondition inner);
    [[nodiscard]] static LootCondition anyOf(std::vector<LootCondition> terms);
    [[nodiscard]] static LootCondition allOf(std::vector<LootCondition> terms);
    [[nodiscard]] static LootCondition randomChanceWithEnchantedBonus(float chance);
    /// `blocks` is a list of ids or a single `#tag`, empty for any block.
    [[nodiscard]] static LootCondition matchBlock(std::vector<std::string> blocks, StateMatcher state,
                                                  bool unsupported = false);
    [[nodiscard]] static LootCondition locationCheck();
    /// parity: `LocationCheck`, the predicate at the origin moved by the offsets.
    [[nodiscard]] static LootCondition locationCheck(LocationPredicate predicate, std::array<double, 3> offset);
    [[nodiscard]] static LootCondition fishingHookOpenWater(bool expected);
    [[nodiscard]] static LootCondition matchTool(ItemPredicate predicate);
    [[nodiscard]] static LootCondition entityMatches(Target target, EntityPredicate predicate);
    /// `entity` or `attacker` or `direct_attacker` present at all, which an empty predicate asks.
    [[nodiscard]] static LootCondition entityExists(Target target);
    [[nodiscard]] static LootCondition tableBonus(std::string enchantment, std::vector<float> chances);
    /// `tags` pairs a damage type tag with whether the damage must carry it.
    [[nodiscard]] static LootCondition damageSource(std::vector<std::pair<std::string, bool>> tags,
                                                    std::optional<EntityPredicate> source,
                                                    std::optional<EntityPredicate> direct);
    [[nodiscard]] static LootCondition randomChanceWithEnchantedBonus(float unenchanted, LevelBasedValue enchanted,
                                                                      std::string enchantment);
    [[nodiscard]] static LootCondition intValueCheck(IntProvider value, IntRange range);
    [[nodiscard]] static LootCondition floatValueCheck(FloatProvider value, FloatRange range);
    [[nodiscard]] static LootCondition enchantmentActiveCheck(bool active);
    [[nodiscard]] static LootCondition timeCheck(wire::common::Identifier clock, std::optional<std::int64_t> period,
                                                 IntRange value);
    [[nodiscard]] static LootCondition weatherCheck(std::optional<bool> raining, std::optional<bool> thundering);
    [[nodiscard]] static LootCondition entityScores(EntityTarget target,
                                                    std::vector<std::pair<std::string, IntRange>> scores);
    /// A number or a flag, written as the number it is or 1 and 0.
    [[nodiscard]] static LootCondition environmentAttributeCheck(wire::common::Identifier attribute, double value);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    [[nodiscard]] bool test(random::RandomSource& random, const LootContext& context = {}) const;

private:
    struct Parts;

    Kind kind_ = Kind::SurvivesExplosion;
    float plainChance_ = 0.0F;
    Target target_ = Target::This;
    std::array<double, 3> offset_{};
    std::shared_ptr<const Parts> parts_;

    [[nodiscard]] static LootCondition build(Kind kind, Parts parts);
    [[nodiscard]] const Parts& parts() const;
    [[nodiscard]] bool testNumbers(random::RandomSource& random, const LootContext& context) const;
    [[nodiscard]] bool testWorld(random::RandomSource& random, const LootContext& context) const;
};

} // namespace basalt::server::world::loot
