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

std::int32_t NumberProvider::getInt(random::RandomSource& random) const {
    if (kind_ == Kind::Constant) {
        return static_cast<std::int32_t>(std::floor(value_));
    }
    const std::int32_t low = min_->getInt(random);
    const std::int32_t high = max_->getInt(random);
    return low >= high ? low : random.nextIntBetweenInclusive(low, high);
}

float NumberProvider::getFloat(random::RandomSource& random) const {
    if (kind_ == Kind::Constant) {
        return value_;
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
    case Kind::SurvivesExplosion:
    case Kind::KilledByPlayer:
    // parity: `LocationCheck` asks about the squares around the block, which this evaluator has no
    // way to look at. Both tables that use one ask whether the other half of a tall plant is still
    // there, and it is: the half being broken is what put this roll here.
    case Kind::LocationCheck:
        return true;
    case Kind::MatchTool:
    case Kind::EntityProperties:
        return false;
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
    case Kind::RandomChanceWithEnchantedBonus:
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

LootFunction LootFunction::applyBonus() {
    LootFunction built;
    built.kind_ = Kind::ApplyBonus;
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
    case Kind::EnchantedCountIncrease:
        // parity: the killer's enchantment level times the roll. Nothing carries one into a loot
        // roll yet, so the level is zero and this adds nothing, which is what an unenchanted kill
        // does in vanilla. `count_` and `limit_` are kept for when a tool reaches this.
        return stack;
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
    case Kind::CopyFromBlock:
    case Kind::FurnaceSmelt:
    // parity: `ApplyBonusCount` scales by the enchantment level on the tool that broke the block,
    // and with a level of zero every one of its three formulas answers the count it was given.
    case Kind::ApplyBonus:
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

void addRandomItem(const LootPool& pool, random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                   const LootPool::Output& output, const LootContext& context) {
    std::vector<const LootEntry*> validEntries;
    std::int32_t totalWeight = 0;
    for (const LootEntry& rawEntry : pool.entries) {
        const LootEntry* entry = resolve(rawEntry, random, context);
        if (entry == nullptr) {
            continue;
        }
        const std::int32_t weight = entry->weightFor(luck);
        if (weight > 0) {
            validEntries.push_back(entry);
            totalWeight += weight;
        }
    }
    if (totalWeight == 0 || validEntries.empty()) {
        return;
    }

    const LootEntry* chosen = validEntries.front();
    if (validEntries.size() > 1) {
        std::int32_t index = random.nextInt(totalWeight);
        for (const LootEntry* entry : validEntries) {
            index -= entry->weightFor(luck);
            if (index < 0) {
                chosen = entry;
                break;
            }
        }
    }

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

std::vector<ItemStack> LootTable::generate(random::RandomSource& random, float luck,
                                           std::optional<float> explosionRadius, const LootContext& context) const {
    std::vector<ItemStack> output;
    for (const LootPool& pool : pools) {
        pool.generate(
            random, luck, explosionRadius,
            [&](ItemStack stack) {
                for (const LootFunction& function : functions) {
                    stack = function.apply(std::move(stack), random, explosionRadius, context);
                }
                // A stack an explosion swallowed whole is not a drop at all.
                if (stack.count > 0) {
                    output.push_back(std::move(stack));
                }
            },
            context);
    }
    return output;
}

} // namespace basalt::server::world::loot
