#include <basalt/server/world/loot/LootCondition.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace basalt::server::world::loot {

struct LootCondition::Parts {
    FloatProvider chance;
    std::vector<LootCondition> terms;
    std::vector<std::string> blocks;
    StateMatcher state;
    bool unsupported = false;
    std::shared_ptr<const ItemPredicate> item;
    std::shared_ptr<const EntityPredicate> entity;
    std::shared_ptr<const EntityPredicate> direct;
    std::shared_ptr<const LocationPredicate> location;
    std::string enchantment;
    std::vector<float> chances;
    LevelBasedValue enchanted;
    std::vector<std::pair<std::string, bool>> damageTags;
    IntProvider intValue;
    FloatProvider floatValue;
    IntRange intRange;
    FloatRange floatRange;
    std::optional<wire::common::Identifier> id;
    std::optional<std::int64_t> period;
    std::optional<bool> raining;
    std::optional<bool> thundering;
    EntityTarget entityTarget = EntityTarget::This;
    std::vector<std::pair<std::string, IntRange>> scores;
    double value = 0.0;
};

namespace {

[[nodiscard]] std::int32_t levelIn(const std::map<std::string, std::int32_t, std::less<>>& levels,
                                   std::string_view enchantment) {
    const auto found = levels.find(enchantment);
    return found == levels.end() ? 0 : found->second;
}

[[nodiscard]] const std::optional<EntityFacts>& factsFor(const LootContext& context, LootCondition::Target target) {
    switch (target) {
    case LootCondition::Target::Attacker:
        return context.attacker;
    case LootCondition::Target::DirectAttacker:
        return context.directAttacker;
    case LootCondition::Target::AttackingPlayer:
        return context.attackingPlayer;
    default:
        return context.thisEntity;
    }
}

} // namespace

bool IntRange::test(random::RandomSource& random, const LootContext& context, std::int32_t value) const {
    if (exact.has_value()) {
        return exact->get(random, context) == value;
    }
    if (min.has_value() && value < min->get(random, context)) {
        return false;
    }
    return !max.has_value() || value <= max->get(random, context);
}

bool FloatRange::test(random::RandomSource& random, const LootContext& context, float value) const {
    if (exact.has_value()) {
        return exact->get(random, context) == value;
    }
    if (min.has_value() && !(value >= min->get(random, context))) {
        return false;
    }
    return !max.has_value() || value <= max->get(random, context);
}

const LootCondition::Parts& LootCondition::parts() const {
    static const Parts kEmpty;
    return parts_ == nullptr ? kEmpty : *parts_;
}

LootCondition LootCondition::build(Kind kind, Parts parts) {
    LootCondition built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

LootCondition LootCondition::randomChance(FloatProvider chance) {
    Parts parts;
    parts.chance = std::move(chance);
    return build(Kind::RandomChance, std::move(parts));
}

LootCondition LootCondition::always() {
    LootCondition built;
    built.kind_ = Kind::Always;
    return built;
}

LootCondition LootCondition::survivesExplosion() {
    LootCondition built;
    built.kind_ = Kind::SurvivesExplosion;
    return built;
}

LootCondition LootCondition::matchTool() {
    LootCondition built;
    built.kind_ = Kind::MatchTool;
    return built;
}

LootCondition LootCondition::entityProperties() {
    LootCondition built;
    built.kind_ = Kind::EntityProperties;
    return built;
}

LootCondition LootCondition::thisEntityExists() {
    LootCondition built;
    built.kind_ = Kind::ThisEntityExists;
    return built;
}

LootCondition LootCondition::entityExists(Target target) {
    LootCondition built;
    built.kind_ = Kind::ThisEntityExists;
    built.target_ = target;
    return built;
}

LootCondition LootCondition::killedByPlayer() {
    LootCondition built;
    built.kind_ = Kind::KilledByPlayer;
    return built;
}

LootCondition LootCondition::inverted(LootCondition inner) {
    Parts parts;
    parts.terms.push_back(std::move(inner));
    return build(Kind::Inverted, std::move(parts));
}

LootCondition LootCondition::anyOf(std::vector<LootCondition> terms) {
    Parts parts;
    parts.terms = std::move(terms);
    return build(Kind::AnyOf, std::move(parts));
}

LootCondition LootCondition::allOf(std::vector<LootCondition> terms) {
    Parts parts;
    parts.terms = std::move(terms);
    return build(Kind::AllOf, std::move(parts));
}

LootCondition LootCondition::matchBlock(std::vector<std::string> blocks, StateMatcher state, bool unsupported) {
    Parts parts;
    parts.blocks = std::move(blocks);
    parts.state = std::move(state);
    parts.unsupported = unsupported;
    return build(Kind::MatchBlock, std::move(parts));
}

LootCondition LootCondition::locationCheck() {
    LootCondition built;
    built.kind_ = Kind::LocationCheck;
    return built;
}

LootCondition LootCondition::locationCheck(LocationPredicate predicate, std::array<double, 3> offset) {
    LootCondition built;
    built.kind_ = Kind::LocationCheck;
    built.offset_ = offset;
    Parts parts;
    parts.location = std::make_shared<const LocationPredicate>(std::move(predicate));
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

LootCondition LootCondition::fishingHookOpenWater(bool expected) {
    LootCondition built;
    built.kind_ = Kind::FishingHookOpenWater;
    built.plainChance_ = expected ? 1.0F : 0.0F;
    return built;
}

LootCondition LootCondition::matchTool(ItemPredicate predicate) {
    Parts parts;
    parts.item = std::make_shared<const ItemPredicate>(std::move(predicate));
    return build(Kind::MatchTool, std::move(parts));
}

LootCondition LootCondition::entityMatches(Target target, EntityPredicate predicate) {
    LootCondition built;
    built.kind_ = Kind::EntityMatches;
    built.target_ = target;
    Parts parts;
    parts.entity = std::make_shared<const EntityPredicate>(std::move(predicate));
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

LootCondition LootCondition::damageSource(std::vector<std::pair<std::string, bool>> tags,
                                          std::optional<EntityPredicate> source,
                                          std::optional<EntityPredicate> direct) {
    Parts parts;
    parts.damageTags = std::move(tags);
    if (source.has_value()) {
        parts.entity = std::make_shared<const EntityPredicate>(std::move(*source));
    }
    if (direct.has_value()) {
        parts.direct = std::make_shared<const EntityPredicate>(std::move(*direct));
    }
    return build(Kind::DamageSource, std::move(parts));
}

LootCondition LootCondition::tableBonus(std::string enchantment, std::vector<float> chances) {
    Parts parts;
    parts.enchantment = std::move(enchantment);
    parts.chances = std::move(chances);
    return build(Kind::TableBonus, std::move(parts));
}

LootCondition LootCondition::randomChanceWithEnchantedBonus(float chance) {
    LootCondition built;
    built.kind_ = Kind::RandomChanceWithEnchantedBonus;
    built.plainChance_ = chance;
    return built;
}

LootCondition LootCondition::randomChanceWithEnchantedBonus(float unenchanted, LevelBasedValue enchanted,
                                                            std::string enchantment) {
    LootCondition built;
    built.kind_ = Kind::RandomChanceWithEnchantedBonus;
    built.plainChance_ = unenchanted;
    Parts parts;
    parts.enchanted = enchanted;
    parts.enchantment = std::move(enchantment);
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

LootCondition LootCondition::intValueCheck(IntProvider value, IntRange range) {
    Parts parts;
    parts.intValue = std::move(value);
    parts.intRange = std::move(range);
    return build(Kind::IntValueCheck, std::move(parts));
}

LootCondition LootCondition::floatValueCheck(FloatProvider value, FloatRange range) {
    Parts parts;
    parts.floatValue = std::move(value);
    parts.floatRange = std::move(range);
    return build(Kind::FloatValueCheck, std::move(parts));
}

LootCondition LootCondition::enchantmentActiveCheck(bool active) {
    LootCondition built;
    built.kind_ = Kind::EnchantmentActiveCheck;
    built.plainChance_ = active ? 1.0F : 0.0F;
    return built;
}

LootCondition LootCondition::timeCheck(wire::common::Identifier clock, std::optional<std::int64_t> period,
                                       IntRange value) {
    Parts parts;
    parts.id = std::move(clock);
    parts.period = period;
    parts.intRange = std::move(value);
    return build(Kind::TimeCheck, std::move(parts));
}

LootCondition LootCondition::weatherCheck(std::optional<bool> raining, std::optional<bool> thundering) {
    Parts parts;
    parts.raining = raining;
    parts.thundering = thundering;
    return build(Kind::WeatherCheck, std::move(parts));
}

LootCondition LootCondition::entityScores(EntityTarget target, std::vector<std::pair<std::string, IntRange>> scores) {
    Parts parts;
    parts.entityTarget = target;
    parts.scores = std::move(scores);
    return build(Kind::EntityScores, std::move(parts));
}

LootCondition LootCondition::environmentAttributeCheck(wire::common::Identifier attribute, double value) {
    Parts parts;
    parts.id = std::move(attribute);
    parts.value = value;
    return build(Kind::EnvironmentAttributeCheck, std::move(parts));
}

bool LootCondition::test(random::RandomSource& random, const LootContext& context) const {
    const Parts& part = parts();
    switch (kind_) {
    case Kind::Always:
        return true;
    // parity: `LootItemRandomChanceCondition`, the chance worked out before the draw.
    case Kind::RandomChance: {
        const float chance = part.chance.get(random, context);
        return random.nextFloat() < chance;
    }
    case Kind::KilledByPlayer:
        return context.killedByPlayer;
    // parity: `ExplosionCondition`, a draw against one over the radius, and true without a blast.
    case Kind::SurvivesExplosion:
        if (!context.explosionRadius.has_value()) {
            return true;
        }
        return random.nextFloat() <= 1.0F / *context.explosionRadius;
    // parity: `LocationCheck`, false without an origin. The bare factory, which carries no predicate,
    // still passes.
    case Kind::LocationCheck: {
        if (part.location == nullptr) {
            return true;
        }
        if (!context.origin.has_value() || !context.placeAt) {
            return false;
        }
        const auto at = [this, &context](std::size_t axis) {
            return static_cast<std::int32_t>(std::floor((*context.origin)[axis] + offset_[axis]));
        };
        return part.location->test(context.placeAt(at(0), at(1), at(2)), context.tags);
    }
    // parity: `MatchTool` with no tool fails; with no predicate any tool passes.
    case Kind::MatchTool:
        return context.tool.has_value() &&
               (part.item == nullptr || part.item->test(context.tool, context.toolEnchantments, context.tags));
    case Kind::EntityMatches: {
        const std::optional<EntityFacts>& entity = factsFor(context, target_);
        return part.entity != nullptr && part.entity->test(entity, context.tags);
    }
    // parity: `DamageSourcePredicate.matches`, false without a damage source at all.
    case Kind::DamageSource: {
        if (!context.damageTagged) {
            return false;
        }
        // parity: 26.3 `TagPredicate` holds a holder set, so `id` is a `#tag` or one damage type.
        for (const auto& [tag, expected] : part.damageTags) {
            const bool held = tag.starts_with('#')
                                  ? context.damageTagged(std::string_view{tag}.substr(1))
                                  : context.damageType.has_value() && context.damageType->toString() == tag;
            if (held != expected) {
                return false;
            }
        }
        return (part.direct == nullptr || part.direct->test(context.directAttacker, context.tags)) &&
               (part.entity == nullptr || part.entity->test(context.attacker, context.tags));
    }
    case Kind::TableBonus: {
        const std::int32_t level = context.tool.has_value() ? levelIn(context.toolEnchantments, part.enchantment) : 0;
        const std::size_t index = std::min(static_cast<std::size_t>(level), part.chances.size() - 1);
        return random.nextFloat() < part.chances[index];
    }
    case Kind::EntityProperties:
        return false;
    case Kind::FishingHookOpenWater:
        return context.inOpenWater.has_value() && *context.inOpenWater == (plainChance_ > 0.0F);
    // parity: `LootItemEntityPropertyCondition` with an empty predicate, which asks only that the
    // entity be there: the player mining the block or the arrow that hit it.
    case Kind::ThisEntityExists:
        return target_ == Target::This ? context.hasEntity || context.thisEntity.has_value()
                                       : factsFor(context, target_).has_value();
    // parity: `MatchBlock`: a roll with no block behind it matches nothing, and a predicate on the
    // block entity is a gap that never matches.
    case Kind::MatchBlock:
        if (part.unsupported || (context.blockState.empty() && !context.block.has_value())) {
            return false;
        }
        if (context.block.has_value() && !matchesIdsOrTag(part.blocks, "block", *context.block, context.tags)) {
            return false;
        }
        return part.state.test(context.blockState);
    case Kind::Inverted:
        return !part.terms.front().test(random, context);
    case Kind::AnyOf:
        return std::ranges::any_of(part.terms, [&](const LootCondition& term) { return term.test(random, context); });
    case Kind::AllOf:
        return std::ranges::all_of(part.terms, [&](const LootCondition& term) { return term.test(random, context); });
    case Kind::RandomChanceWithEnchantedBonus: {
        const std::int32_t level =
            context.attacker.has_value() ? levelIn(context.attackerEnchantments, part.enchantment) : 0;
        return random.nextFloat() < (level > 0 ? part.enchanted.at(level) : plainChance_);
    }
    default:
        return testNumbers(random, context);
    }
}

bool LootCondition::testNumbers(random::RandomSource& random, const LootContext& context) const {
    const Parts& part = parts();
    switch (kind_) {
    // parity: `IntValueCheck` and `FloatValueCheck`: the value first, then the range's own bounds.
    case Kind::IntValueCheck: {
        const std::int32_t value = part.intValue.get(random, context);
        return part.intRange.test(random, context, value);
    }
    case Kind::FloatValueCheck: {
        const float value = part.floatValue.get(random, context);
        return part.floatRange.test(random, context, value);
    }
    case Kind::EnchantmentActiveCheck:
        return context.enchantmentActive.has_value() && *context.enchantmentActive == (plainChance_ > 0.0F);
    default:
        return testWorld(random, context);
    }
}

bool LootCondition::testWorld(random::RandomSource& random, const LootContext& context) const {
    const Parts& part = parts();
    switch (kind_) {
    // parity: `TimeCheck`, the clock's ticks folded by the period and narrowed to an int.
    case Kind::TimeCheck: {
        const std::optional<std::int64_t> ticks = context.clockTicks ? context.clockTicks(*part.id) : std::nullopt;
        if (!ticks.has_value()) {
            return false;
        }
        std::int64_t time = *ticks;
        if (part.period.has_value() && *part.period != 0) {
            time %= *part.period;
        }
        return part.intRange.test(random, context, static_cast<std::int32_t>(time));
    }
    case Kind::WeatherCheck:
        if (part.raining.has_value() && context.raining.value_or(false) != *part.raining) {
            return false;
        }
        return !part.thundering.has_value() || context.thundering.value_or(false) == *part.thundering;
    // parity: `EntityHasScoreCondition`: every objective scored and in range, in the file's order.
    case Kind::EntityScores: {
        const std::optional<std::string> holder = scoreHolderOf(context, part.entityTarget);
        if (!holder.has_value()) {
            return false;
        }
        for (const auto& [objective, range] : part.scores) {
            const std::optional<std::int32_t> score = context.score ? context.score(*holder, objective) : std::nullopt;
            if (!score.has_value() || !range.test(random, context, *score)) {
                return false;
            }
        }
        return true;
    }
    case Kind::EnvironmentAttributeCheck: {
        const std::optional<double> value = context.environment ? context.environment(*part.id) : std::nullopt;
        return value.has_value() && *value == part.value;
    }
    default:
        return random.nextFloat() < plainChance_;
    }
}

} // namespace basalt::server::world::loot
