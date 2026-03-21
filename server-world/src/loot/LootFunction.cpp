#include "LootParts.hpp"

#include <basalt/server/registry/Items.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::world::loot {
namespace {

[[nodiscard]] std::int32_t levelIn(const std::map<std::string, std::int32_t, std::less<>>& levels,
                                   std::string_view enchantment) {
    const auto found = levels.find(enchantment);
    return found == levels.end() ? 0 : found->second;
}

} // namespace

const LootFunction::Parts& LootFunction::parts() const {
    static const Parts kEmpty;
    return parts_ == nullptr ? kEmpty : *parts_;
}

LootFunction LootFunction::enchantedCountIncrease(FloatProvider count, std::optional<std::int32_t> limit) {
    return enchantedCountIncrease(std::string{}, std::move(count), limit);
}

LootFunction LootFunction::enchantedCountIncrease(std::string enchantment, FloatProvider count,
                                                  std::optional<std::int32_t> limit) {
    LootFunction built;
    built.kind_ = Kind::EnchantedCountIncrease;
    built.limit_ = limit;
    built.parts_ =
        std::make_shared<const Parts>(Parts{.fraction = std::move(count), .enchantment = std::move(enchantment)});
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

LootFunction LootFunction::smelt(bool useInputCount) {
    LootFunction built;
    built.kind_ = Kind::Smelt;
    built.add_ = useInputCount;
    return built;
}

LootFunction LootFunction::discard() {
    LootFunction built;
    built.kind_ = Kind::Discard;
    return built;
}

LootFunction LootFunction::setCount(IntProvider count, bool add) {
    LootFunction built;
    built.kind_ = Kind::SetCount;
    built.add_ = add;
    built.parts_ = std::make_shared<const Parts>(Parts{.count = std::move(count)});
    return built;
}

LootFunction LootFunction::applyBonus(std::string enchantment, BonusFormula formula, std::int32_t multiplierOrExtra,
                                      float probability) {
    LootFunction built;
    built.kind_ = Kind::ApplyBonus;
    built.formula_ = formula;
    built.bonusNumber_ = multiplierOrExtra;
    built.probability_ = probability;
    built.parts_ = std::make_shared<const Parts>(Parts{.enchantment = std::move(enchantment)});
    return built;
}

LootFunction LootFunction::limitCount(std::optional<IntProvider> least, std::optional<IntProvider> most) {
    LootFunction built;
    built.kind_ = Kind::LimitCount;
    built.parts_ = std::make_shared<const Parts>(Parts{.least = std::move(least), .most = std::move(most)});
    return built;
}

LootFunction LootFunction::sequence(std::vector<LootFunction> functions) {
    LootFunction built;
    built.kind_ = Kind::Sequence;
    built.parts_ = std::make_shared<const Parts>(Parts{.functions = std::move(functions)});
    return built;
}

LootFunction LootFunction::filtered(ItemPredicate filter, std::optional<LootFunction> onPass,
                                    std::optional<LootFunction> onFail) {
    Parts parts{.filter = std::move(filter)};
    if (onPass.has_value()) {
        parts.onPass.push_back(std::move(*onPass));
    }
    if (onFail.has_value()) {
        parts.onFail.push_back(std::move(*onFail));
    }
    LootFunction built;
    built.kind_ = Kind::Filtered;
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

LootFunction LootFunction::setOminousBottleAmplifier(IntProvider amplifier) {
    LootFunction built;
    built.kind_ = Kind::SetOminousBottleAmplifier;
    built.parts_ = std::make_shared<const Parts>(Parts{.count = std::move(amplifier)});
    return built;
}

LootFunction LootFunction::setRandomPotion(std::vector<std::string> options) {
    LootFunction built;
    built.kind_ = Kind::SetRandomPotion;
    built.parts_ = std::make_shared<const Parts>(Parts{.options = std::move(options)});
    return built;
}

LootFunction LootFunction::setRandomDyes(IntProvider count) {
    LootFunction built;
    built.kind_ = Kind::SetRandomDyes;
    built.parts_ = std::make_shared<const Parts>(Parts{.count = std::move(count)});
    return built;
}

ItemStack LootFunction::apply(ItemStack stack, random::RandomSource& random, std::optional<float> explosionRadius,
                              const LootContext& context) const {
    if (explosionRadius.has_value() && !context.explosionRadius.has_value()) {
        LootContext blasted = context;
        blasted.explosionRadius = explosionRadius;
        return apply(std::move(stack), random, std::nullopt, blasted);
    }
    // parity: `LootItemConditionalFunction.apply`, which runs its condition first and hands the
    // stack back untouched when it says no.
    for (const LootCondition& condition : conditions) {
        if (!condition.test(random, context)) {
            return stack;
        }
    }
    return run(std::move(stack), random, explosionRadius, context);
}

ItemStack LootFunction::run(ItemStack stack, random::RandomSource& random, std::optional<float> /*explosionRadius*/,
                            const LootContext& context) const {
    const Parts& part = parts();
    switch (kind_) {
    // parity: `EnchantedCountIncreaseFunction`: the attacker's level times the roll, rounded, then
    // the optional cap; nothing at all, the roll included, without a living attacker or the level.
    case Kind::EnchantedCountIncrease: {
        const std::int32_t level =
            context.attacker.has_value() ? levelIn(context.attackerEnchantments, part.enchantment) : 0;
        if (level == 0) {
            return stack;
        }
        const float addition = static_cast<float>(level) * part.fraction.get(random, context);
        stack.count += lootMath::round(addition);
        if (limit_.has_value() && *limit_ > 0) {
            stack.count = std::min(stack.count, *limit_);
        }
        return stack;
    }
    // parity: `ApplyExplosionDecay`, one draw per item against one over the radius.
    case Kind::ExplosionDecay: {
        if (!context.explosionRadius.has_value()) {
            return stack;
        }
        const float chance = 1.0F / *context.explosionRadius;
        std::int32_t kept = 0;
        for (std::int32_t index = 0; index < stack.count; ++index) {
            if (random.nextFloat() <= chance) {
                ++kept;
            }
        }
        stack.count = kept;
        return stack;
    }
    // parity: `ApplyBonusCount`, by the level of the enchantment on the tool, and only with a tool.
    case Kind::ApplyBonus: {
        if (!context.tool.has_value()) {
            return stack;
        }
        const std::int32_t level = levelIn(context.toolEnchantments, part.enchantment);
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
    // parity: `SmeltItemFunction`, the recipe's result times the input count, capped at a stack.
    case Kind::Smelt: {
        if (stack.count <= 0 || !context.smelt) {
            return stack;
        }
        const std::optional<ItemStack> result = context.smelt(stack);
        if (!result.has_value() || result->count <= 0) {
            return stack;
        }
        ItemStack smelted = *result;
        const std::int32_t wanted = (add_ ? stack.count : 1) * result->count;
        smelted.count = std::min(wanted, server::registry::itemStacking::maxStackSize(smelted.item));
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
    case Kind::SetOminousBottleAmplifier:
    case Kind::SetRandomPotion:
    case Kind::SetRandomDyes:
        return setComponent(std::move(stack), random, context);
    case Kind::CopyFromBlock:
        return stack;
    // parity: `IntLimit.clamp`: `Mth.clamp` with both ends, else whichever end there is.
    case Kind::LimitCount: {
        if (part.least.has_value() && part.most.has_value()) {
            const std::int32_t least = part.least->get(random, context);
            const std::int32_t most = part.most->get(random, context);
            stack.count = stack.count < least ? least : std::min(stack.count, most);
        } else if (part.least.has_value()) {
            stack.count = std::max(part.least->get(random, context), stack.count);
        } else if (part.most.has_value()) {
            stack.count = std::min(part.most->get(random, context), stack.count);
        }
        return stack;
    }
    case Kind::Sequence:
        for (const LootFunction& function : part.functions) {
            stack = function.apply(std::move(stack), random, std::nullopt, context);
        }
        return stack;
    case Kind::Filtered: {
        const bool passes = part.filter.test(stack.item, stack.components, context.tags);
        for (const LootFunction& function : passes ? part.onPass : part.onFail) {
            stack = function.apply(std::move(stack), random, std::nullopt, context);
        }
        return stack;
    }
    case Kind::Discard:
        return ItemStack{.item = wire::common::Identifier::vanilla("air"), .count = 0};
    case Kind::SetCount:
    default: {
        const std::int32_t base = add_ ? stack.count : 0;
        stack.count = base + part.count.get(random, context);
        return stack;
    }
    }
}

} // namespace basalt::server::world::loot
