#include <basalt/server/world/loot/Loot.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::world::loot {

NumberProvider NumberProvider::constant(float value) {
    NumberProvider built;
    built.kind_ = Kind::Constant;
    built.value_ = value;
    return built;
}

NumberProvider NumberProvider::uniform(NumberProvider min, NumberProvider max) {
    NumberProvider built;
    built.kind_ = Kind::Uniform;
    built.min_ = std::make_shared<const NumberProvider>(std::move(min));
    built.max_ = std::make_shared<const NumberProvider>(std::move(max));
    return built;
}

NumberProvider NumberProvider::binomial(NumberProvider n, NumberProvider p) {
    NumberProvider built;
    built.kind_ = Kind::Binomial;
    built.min_ = std::make_shared<const NumberProvider>(std::move(n));
    built.max_ = std::make_shared<const NumberProvider>(std::move(p));
    return built;
}

std::int32_t NumberProvider::getInt(random::RandomSource& random) const {
    if (kind_ == Kind::Constant) {
        return static_cast<std::int32_t>(std::floor(value_));
    }
    if (kind_ == Kind::Binomial) {
        const std::int32_t tries = min_->getInt(random);
        const float chance = max_->getFloat(random);
        std::int32_t hits = 0;
        for (std::int32_t index = 0; index < tries; ++index) {
            if (random.nextFloat() < chance) {
                ++hits;
            }
        }
        return hits;
    }
    const std::int32_t low = min_->getInt(random);
    const std::int32_t high = max_->getInt(random);
    return low >= high ? low : random.nextIntBetweenInclusive(low, high);
}

float NumberProvider::getFloat(random::RandomSource& random) const {
    if (kind_ == Kind::Constant) {
        return value_;
    }
    if (kind_ == Kind::Binomial) {
        return static_cast<float>(getInt(random));
    }
    const float low = min_->getFloat(random);
    const float high = max_->getFloat(random);
    return low >= high ? low : low + random.nextFloat() * (high - low);
}

LootCondition LootCondition::randomChance(NumberProvider chance) {
    LootCondition built;
    built.kind_ = Kind::RandomChance;
    built.chance_ = std::move(chance);
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

LootCondition LootCondition::killedByPlayer() {
    LootCondition built;
    built.kind_ = Kind::KilledByPlayer;
    return built;
}

LootCondition LootCondition::inverted(LootCondition inner) {
    LootCondition built;
    built.kind_ = Kind::Inverted;
    built.inner_ = std::make_shared<const LootCondition>(std::move(inner));
    return built;
}

LootCondition LootCondition::anyOf(std::vector<LootCondition> terms) {
    LootCondition built;
    built.kind_ = Kind::AnyOf;
    built.terms_ = std::move(terms);
    return built;
}

LootCondition LootCondition::allOf(std::vector<LootCondition> terms) {
    LootCondition built;
    built.kind_ = Kind::AllOf;
    built.terms_ = std::move(terms);
    return built;
}

LootCondition LootCondition::blockStateProperty(std::map<std::string, std::string> expected) {
    LootCondition built;
    built.kind_ = Kind::BlockStateProperty;
    built.expected_ = std::move(expected);
    return built;
}

LootCondition LootCondition::locationCheck() {
    LootCondition built;
    built.kind_ = Kind::LocationCheck;
    return built;
}

LootCondition LootCondition::locationCheck(LocationPredicate predicate, std::array<double, 3> offset) {
    LootCondition built;
    built.kind_ = Kind::LocationCheck;
    built.location_ = std::make_shared<const LocationPredicate>(std::move(predicate));
    built.offset_ = offset;
    return built;
}

LootCondition LootCondition::fishingHookOpenWater(bool expected) {
    LootCondition built;
    built.kind_ = Kind::FishingHookOpenWater;
    built.plainChance_ = expected ? 1.0F : 0.0F;
    return built;
}

LootCondition LootCondition::matchTool(ItemPredicate predicate) {
    LootCondition built;
    built.kind_ = Kind::MatchTool;
    built.item_ = std::make_shared<const ItemPredicate>(std::move(predicate));
    return built;
}

LootCondition LootCondition::entityMatches(Target target, EntityPredicate predicate) {
    LootCondition built;
    built.kind_ = Kind::EntityMatches;
    built.target_ = target;
    built.entity_ = std::make_shared<const EntityPredicate>(std::move(predicate));
    return built;
}

LootCondition LootCondition::damageSource(std::vector<std::pair<std::string, bool>> tags,
                                          std::optional<EntityPredicate> source,
                                          std::optional<EntityPredicate> direct) {
    LootCondition built;
    built.kind_ = Kind::DamageSource;
    built.damageTags_ = std::move(tags);
    if (source.has_value()) {
        built.entity_ = std::make_shared<const EntityPredicate>(std::move(*source));
    }
    if (direct.has_value()) {
        built.direct_ = std::make_shared<const EntityPredicate>(std::move(*direct));
    }
    return built;
}

LootCondition LootCondition::tableBonus(std::string enchantment, std::vector<float> chances) {
    LootCondition built;
    built.kind_ = Kind::TableBonus;
    built.enchantment_ = std::move(enchantment);
    built.chances_ = std::move(chances);
    return built;
}

LootCondition LootCondition::randomChanceWithEnchantedBonus(float unenchanted, LevelBasedValue enchanted,
                                                            std::string enchantment) {
    LootCondition built;
    built.kind_ = Kind::RandomChanceWithEnchantedBonus;
    built.plainChance_ = unenchanted;
    built.enchanted_ = enchanted;
    built.enchantment_ = std::move(enchantment);
    return built;
}

namespace {

[[nodiscard]] std::int32_t levelIn(const std::map<std::string, std::int32_t, std::less<>>& levels,
                                   std::string_view enchantment) {
    const auto found = levels.find(enchantment);
    return found == levels.end() ? 0 : found->second;
}

} // namespace

LootCondition LootCondition::randomChanceWithEnchantedBonus(float chance) {
    LootCondition built;
    built.kind_ = Kind::RandomChanceWithEnchantedBonus;
    built.plainChance_ = chance;
    return built;
}

bool LootCondition::test(random::RandomSource& random, const LootContext& context) const {
    switch (kind_) {
    case Kind::RandomChance:
        return random.nextFloat() < chance_.getFloat(random);
    case Kind::KilledByPlayer:
        return context.killedByPlayer;
    case Kind::SurvivesExplosion:
    // parity: `LocationCheck`, false without an origin. The bare factory, which carries no predicate,
    // still passes.
    case Kind::LocationCheck: {
        if (location_ == nullptr) {
            return true;
        }
        if (!context.origin.has_value() || !context.placeAt) {
            return false;
        }
        const auto at = [this, &context](std::size_t axis) {
            return static_cast<std::int32_t>(std::floor((*context.origin)[axis] + offset_[axis]));
        };
        return location_->test(context.placeAt(at(0), at(1), at(2)), context.tags);
    }
    // parity: `MatchTool` with no tool fails, and so does a predicate this build cannot read.
    case Kind::MatchTool:
        return item_ != nullptr && item_->test(context.tool, context.toolEnchantments, context.tags);
    case Kind::EntityMatches: {
        const std::optional<EntityFacts>& entity = target_ == Target::This       ? context.thisEntity
                                                   : target_ == Target::Attacker ? context.attacker
                                                                                 : context.directAttacker;
        return entity_ != nullptr && entity_->test(entity, context.tags);
    }
    // parity: `DamageSourcePredicate.matches`, false without a damage source at all.
    case Kind::DamageSource: {
        if (!context.damageTagged) {
            return false;
        }
        for (const auto& [tag, expected] : damageTags_) {
            if (context.damageTagged(tag) != expected) {
                return false;
            }
        }
        return (direct_ == nullptr || direct_->test(context.directAttacker, context.tags)) &&
               (entity_ == nullptr || entity_->test(context.attacker, context.tags));
    }
    case Kind::TableBonus: {
        const std::int32_t level = levelIn(context.toolEnchantments, enchantment_);
        const std::size_t index = std::min(static_cast<std::size_t>(level), chances_.size() - 1);
        return random.nextFloat() < chances_[index];
    }
    case Kind::EntityProperties:
        return false;
    case Kind::FishingHookOpenWater:
        return context.inOpenWater.has_value() && *context.inOpenWater == (plainChance_ > 0.0F);
    // parity: `LootItemEntityPropertyCondition` with an empty predicate, which asks only that the
    // entity be there: the player mining the block or the arrow that hit it.
    case Kind::ThisEntityExists:
        return context.hasEntity;
    // parity: `LootItemBlockStatePropertyCondition`, every named property of the block that broke
    // matching what the table asks for. A roll with no block behind it matches nothing.
    case Kind::BlockStateProperty:
        return std::all_of(expected_.begin(), expected_.end(), [&context](const auto& want) {
            const auto found = context.blockState.find(want.first);
            return found != context.blockState.end() && found->second == want.second;
        });
    case Kind::Inverted:
        return !inner_->test(random, context);
    case Kind::AnyOf:
        return std::any_of(terms_.begin(), terms_.end(),
                           [&](const LootCondition& term) { return term.test(random, context); });
    case Kind::AllOf:
        return std::all_of(terms_.begin(), terms_.end(),
                           [&](const LootCondition& term) { return term.test(random, context); });
    case Kind::RandomChanceWithEnchantedBonus: {
        const std::int32_t level = levelIn(context.attackerEnchantments, enchantment_);
        return random.nextFloat() < (level > 0 ? enchanted_.at(level) : plainChance_);
    }
    default:
        return random.nextFloat() < plainChance_;
    }
}

LootFunction LootFunction::enchantedCountIncrease(NumberProvider count, std::optional<std::int32_t> limit) {
    LootFunction built;
    built.kind_ = Kind::EnchantedCountIncrease;
    built.count_ = std::move(count);
    built.limit_ = limit;
    return built;
}

LootFunction LootFunction::explosionDecay() {
    LootFunction built;
    built.kind_ = Kind::ExplosionDecay;
    return built;
}

LootFunction LootFunction::copyFromBlock() {
    LootFunction built;
    built.kind_ = Kind::CopyFromBlock;
    return built;
}

LootFunction LootFunction::furnaceSmelt() {
    LootFunction built;
    built.kind_ = Kind::FurnaceSmelt;
    return built;
}

LootFunction LootFunction::smelt() {
    LootFunction built;
    built.kind_ = Kind::Smelt;
    return built;
}

LootFunction LootFunction::applyBonus() {
    LootFunction built;
    built.kind_ = Kind::ApplyBonus;
    return built;
}

LootFunction LootFunction::applyBonus(std::string enchantment, BonusFormula formula, std::int32_t multiplierOrExtra,
                                      float probability) {
    LootFunction built;
    built.kind_ = Kind::ApplyBonus;
    built.enchantment_ = std::move(enchantment);
    built.formula_ = formula;
    built.bonusNumber_ = multiplierOrExtra;
    built.probability_ = probability;
    return built;
}

LootFunction LootFunction::enchantedCountIncrease(std::string enchantment, NumberProvider count,
                                                  std::optional<std::int32_t> limit) {
    LootFunction built = enchantedCountIncrease(std::move(count), limit);
    built.enchantment_ = std::move(enchantment);
    return built;
}

LootFunction LootFunction::limitCount(std::optional<std::int32_t> least, std::optional<std::int32_t> most) {
    LootFunction built;
    built.kind_ = Kind::LimitCount;
    built.least_ = least;
    built.limit_ = most;
    return built;
}

LootFunction LootFunction::setCount(NumberProvider count, bool add) {
    LootFunction built;
    built.kind_ = Kind::SetCount;
    built.count_ = std::move(count);
    built.add_ = add;
    return built;
}

ItemStack LootFunction::apply(ItemStack stack, random::RandomSource& random, std::optional<float> explosionRadius,
                              const LootContext& context) const {
    // parity: `LootItemConditionalFunction.apply`, which runs its conditions first and hands the
    // stack back untouched when any of them says no.
    for (const LootCondition& condition : conditions) {
        if (!condition.test(random, context)) {
            return stack;
        }
    }
    switch (kind_) {
    case Kind::EnchantedCountIncrease: {
        // parity: the attacker's level times the roll, rounded, then the optional cap.
        const std::int32_t level =
            context.attacker.has_value() ? levelIn(context.attackerEnchantments, enchantment_) : 0;
        if (level == 0) {
            return stack;
        }
        // parity: `Math.round(float)`, half up.
        stack.count +=
            static_cast<std::int32_t>(std::floor((static_cast<float>(level) * count_.getFloat(random)) + 0.5F));
        if (limit_.has_value() && *limit_ > 0) {
            stack.count = std::min(stack.count, *limit_);
        }
        return stack;
    }
    case Kind::ExplosionDecay: {
        if (!explosionRadius.has_value() || *explosionRadius <= 0.0F) {
            return stack;
        }
        const float chance = 1.0F / *explosionRadius;
        std::int32_t kept = 0;
        for (std::int32_t index = 0; index < stack.count; ++index) {
            if (random.nextFloat() <= chance) {
                ++kept;
            }
        }
        stack.count = std::max(kept, 0);
        return stack;
    }
    case Kind::ApplyBonus: {
        // parity: `ApplyBonusCount`, by the level of the enchantment on the tool.
        const std::int32_t level = levelIn(context.toolEnchantments, enchantment_);
        switch (formula_) {
        case BonusFormula::OreDrops:
            if (level > 0) {
                stack.count *= std::max(random.nextInt(level + 2) - 1, 0) + 1;
            }
            break;
        case BonusFormula::UniformBonusCount:
            stack.count += random.nextInt((bonusNumber_ * level) + 1);
            break;
        case BonusFormula::BinomialWithBonusCount:
            for (std::int32_t index = 0; index < level + bonusNumber_; ++index) {
                if (random.nextFloat() < probability_) {
                    stack.count++;
                }
            }
            break;
        }
        return stack;
    }
    case Kind::Smelt: {
        // parity: `SmeltItemFunction.run`, the result with the input's count, or the input unchanged.
        if (stack.count <= 0 || !context.smelt) {
            return stack;
        }
        const std::optional<ItemStack> result = context.smelt(stack);
        if (!result.has_value()) {
            return stack;
        }
        ItemStack smelted = *result;
        smelted.count = stack.count;
        return smelted;
    }
    case Kind::EnchantRandomly:
    case Kind::EnchantWithLevels:
    case Kind::SetEnchantments:
        return enchant(std::move(stack), random, context);
    case Kind::SetDamage:
    case Kind::SetComponents:
    case Kind::SetStewEffect:
    case Kind::SetInstrument:
        return setComponent(std::move(stack), random, context);
    case Kind::CopyFromBlock:
    case Kind::FurnaceSmelt:
        return stack;
    // parity: `LimitCount`, the clamp on whatever the functions before it left.
    case Kind::LimitCount:
        if (least_.has_value()) {
            stack.count = std::max(stack.count, *least_);
        }
        if (limit_.has_value()) {
            stack.count = std::min(stack.count, *limit_);
        }
        return stack;
    case Kind::SetCount:
    default: {
        const std::int32_t base = add_ ? stack.count : 0;
        stack.count = base + count_.getInt(random);
        return stack;
    }
    }
}

std::int32_t LootEntry::weightFor(float luck) const {
    return std::max(
        static_cast<std::int32_t>(std::floor(static_cast<float>(weight) + static_cast<float>(quality) * luck)), 0);
}

bool LootEntry::canRun(random::RandomSource& random, const LootContext& context) const {
    return std::all_of(conditions.begin(), conditions.end(),
                       [&](const LootCondition& condition) { return condition.test(random, context); });
}

ItemStack LootEntry::createItemStack(random::RandomSource& random, const LootContext& context) const {
    ItemStack stack{.item = item.value_or(wire::common::Identifier::vanilla("air")), .count = 1};
    for (const LootFunction& function : functions) {
        stack = function.apply(std::move(stack), random, std::nullopt, context);
    }
    return stack;
}

namespace {

/// Flattens a composite `alternatives` entry down to the leaf the pool should weigh. parity:
/// `LootPoolEntryContainer.expand`: the container's own conditions gate the whole thing, then its
/// children are tried in order and the first whose own conditions pass wins, short-circuit rather
/// than weighted among the alternatives.
[[nodiscard]] const LootEntry* resolve(const LootEntry& entry, random::RandomSource& random,
                                       const LootContext& context) {
    if (!entry.canRun(random, context)) {
        return nullptr;
    }
    if (entry.alternatives.empty()) {
        return &entry;
    }
    for (const LootEntry& child : entry.alternatives) {
        const LootEntry* resolved = resolve(child, random, context);
        if (resolved != nullptr) {
            return resolved;
        }
    }
    return nullptr;
}

/// A tag's items, in the tag's order.
[[nodiscard]] std::vector<wire::common::Identifier> tagItems(const LootEntry& entry, const LootContext& context) {
    return context.tagMembers ? context.tagMembers("item", *entry.tag) : std::vector<wire::common::Identifier>{};
}

/// One weighed choice: an entry, or one item of an expanded tag entry.
struct Choice {
    const LootEntry* entry = nullptr;
    std::optional<wire::common::Identifier> item;
};

void addRandomItem(const LootPool& pool, random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                   const LootPool::Output& output, const LootContext& context) {
    std::vector<Choice> validEntries;
    std::int32_t totalWeight = 0;
    for (const LootEntry& rawEntry : pool.entries) {
        const LootEntry* entry = resolve(rawEntry, random, context);
        if (entry == nullptr) {
            continue;
        }
        const std::int32_t weight = entry->weightFor(luck);
        if (weight <= 0) {
            continue;
        }
        // parity: `TagEntry.expand`, one choice per item of the tag, each weighing what the entry does.
        if (entry->tag.has_value() && entry->expand) {
            for (const wire::common::Identifier& item : tagItems(*entry, context)) {
                validEntries.push_back(Choice{.entry = entry, .item = item});
                totalWeight += weight;
            }
            continue;
        }
        validEntries.push_back(Choice{.entry = entry});
        totalWeight += weight;
    }
    if (totalWeight == 0 || validEntries.empty()) {
        return;
    }

    Choice picked = validEntries.front();
    if (validEntries.size() > 1) {
        std::int32_t index = random.nextInt(totalWeight);
        for (const Choice& choice : validEntries) {
            index -= choice.entry->weightFor(luck);
            if (index < 0) {
                picked = choice;
                break;
            }
        }
    }
    const LootEntry* chosen = picked.entry;

    const auto finish = [&](ItemStack stack) {
        for (const LootFunction& function : pool.functions) {
            stack = function.apply(std::move(stack), random, explosionRadius, context);
        }
        output(std::move(stack));
    };
    // parity: `DynamicLoot.createItemStack` hands the consumer whatever the block put in, which is
    // several stacks and not one: a pot leaves four sherds through a single entry.
    if (chosen->dynamicName.has_value()) {
        const auto found = context.dynamic.find(*chosen->dynamicName);
        if (found == context.dynamic.end()) {
            return;
        }
        for (const ItemStack& piece : found->second) {
            ItemStack stack = piece;
            for (const LootFunction& function : chosen->functions) {
                stack = function.apply(std::move(stack), random, std::nullopt, context);
            }
            finish(std::move(stack));
        }
        return;
    }
    // parity: `NestedLootTable.createItemStack`, the other table rolled on this same context and
    // random, its stacks going through this entry's functions.
    if (chosen->table.has_value() || chosen->inlineTable != nullptr) {
        const LootTable* nested = chosen->inlineTable != nullptr ? chosen->inlineTable.get()
                                  : context.tables               ? context.tables(*chosen->table)
                                                                 : nullptr;
        if (nested == nullptr) {
            return;
        }
        nested->generateRaw(
            random, luck, explosionRadius,
            [&](ItemStack stack) {
                for (const LootFunction& function : chosen->functions) {
                    stack = function.apply(std::move(stack), random, std::nullopt, context);
                }
                finish(std::move(stack));
            },
            context);
        return;
    }
    const auto decorated = [&](const wire::common::Identifier& item) {
        ItemStack stack{.item = item, .count = 1};
        for (const LootFunction& function : chosen->functions) {
            stack = function.apply(std::move(stack), random, std::nullopt, context);
        }
        finish(std::move(stack));
    };
    if (picked.item.has_value()) {
        decorated(*picked.item);
        return;
    }
    // parity: `TagEntry.createItemStack` without expanding, every item of the tag at once.
    if (chosen->tag.has_value()) {
        for (const wire::common::Identifier& item : tagItems(*chosen, context)) {
            decorated(item);
        }
        return;
    }
    // parity: `EmptyLootItem`, which hands the consumer nothing.
    if (chosen->empty) {
        return;
    }
    finish(chosen->createItemStack(random, context));
}

} // namespace

void LootPool::generate(random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                        const Output& output, const LootContext& context) const {
    for (const LootCondition& condition : conditions) {
        if (!condition.test(random, context)) {
            return;
        }
    }
    const std::int32_t count =
        rolls.getInt(random) + static_cast<std::int32_t>(std::floor(bonusRolls.getFloat(random) * luck));
    for (std::int32_t index = 0; index < count; ++index) {
        addRandomItem(*this, random, luck, explosionRadius, output, context);
    }
}

void LootTable::generateRaw(random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                            const LootPool::Output& output, const LootContext& context) const {
    for (const LootPool& pool : pools) {
        pool.generate(
            random, luck, explosionRadius,
            [&](ItemStack stack) {
                for (const LootFunction& function : functions) {
                    stack = function.apply(std::move(stack), random, explosionRadius, context);
                }
                output(std::move(stack));
            },
            context);
    }
}

std::vector<ItemStack> LootTable::generate(random::RandomSource& random, float luck,
                                           std::optional<float> explosionRadius, const LootContext& context) const {
    std::vector<ItemStack> output;
    generateRaw(
        random, luck, explosionRadius,
        [&output](ItemStack stack) {
            // A stack an explosion swallowed whole is not a drop at all.
            if (stack.count > 0) {
                output.push_back(std::move(stack));
            }
        },
        context);
    return output;
}

} // namespace basalt::server::world::loot
