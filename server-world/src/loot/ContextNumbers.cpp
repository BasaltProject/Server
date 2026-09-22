#include <basalt/server/world/loot/ContextNumbers.hpp>
#include <basalt/server/world/loot/LootCondition.hpp>
#include <basalt/wire/common/JavaCast.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace basalt::server::world::loot {

struct IntProvider::Parts {
    std::vector<IntProvider> ints;
    std::vector<FloatProvider> floats;
    std::vector<LootCondition> conditions;
    std::vector<std::int32_t> weights;
    ScoreTarget target;
    std::string text;
    std::optional<wire::common::Identifier> id;
};

struct FloatProvider::Parts {
    std::vector<FloatProvider> floats;
    std::vector<IntProvider> ints;
    std::vector<LootCondition> conditions;
    std::vector<std::int32_t> weights;
    LevelBasedValue level;
    std::string text;
    std::optional<wire::common::Identifier> id;
};

namespace {

using wire::common::toIntLikeJava;
using wire::common::toLongLikeJava;

constexpr std::int32_t kIntMin = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t kIntMax = std::numeric_limits<std::int32_t>::max();

/// parity: `ContextIntProvider.longToIntSafe`, which throws rather than wrapping.
[[nodiscard]] std::optional<std::int32_t> longToInt(std::int64_t value) {
    return value < kIntMin || value > kIntMax ? std::nullopt : std::optional{static_cast<std::int32_t>(value)};
}

/// Java's `long` arithmetic, which wraps silently where C++ would be undefined.
[[nodiscard]] std::int64_t wrappingAdd(std::int64_t left, std::int64_t right) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
}

[[nodiscard]] std::int64_t wrappingMultiply(std::int64_t left, std::int64_t right) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(left) * static_cast<std::uint64_t>(right));
}

/// parity: `Math.powExact(int, int)`: a negative exponent or an overflow throws.
[[nodiscard]] std::optional<std::int32_t> powExact(std::int32_t base, std::int32_t exponent) {
    if (exponent < 0) {
        return std::nullopt;
    }
    // Only these three survive a large exponent, so they are answered before the loop.
    if (base == 0 || base == 1) {
        return exponent == 0 ? 1 : base;
    }
    if (base == -1) {
        return exponent % 2 == 0 ? 1 : -1;
    }
    std::int64_t result = 1;
    for (std::int32_t step = 0; step < exponent; ++step) {
        result *= base;
        if (result < kIntMin || result > kIntMax) {
            return std::nullopt;
        }
    }
    return static_cast<std::int32_t>(result);
}

/// parity: `Math.floorDiv` and `Math.floorMod` for ints.
[[nodiscard]] std::int32_t floorDiv(std::int32_t left, std::int32_t right) {
    std::int32_t quotient = left / right;
    if ((left % right != 0) && ((left < 0) != (right < 0))) {
        --quotient;
    }
    return quotient;
}

/// parity: Java's `Math.max(float, float)`: NaN wins, and `0.0` beats `-0.0`.
[[nodiscard]] float javaMax(float left, float right) {
    if (std::isnan(left) || std::isnan(right)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (left == right) {
        return std::signbit(left) ? right : left;
    }
    return left > right ? left : right;
}

[[nodiscard]] float javaMin(float left, float right) {
    if (std::isnan(left) || std::isnan(right)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (left == right) {
        return std::signbit(left) ? left : right;
    }
    return left < right ? left : right;
}

/// parity: `Math.round(float)`, half up, saturating, and 0 for NaN.
[[nodiscard]] std::int32_t roundFloat(float value) {
    return toIntLikeJava(std::floor(static_cast<double>(value) + 0.5));
}

/// parity: `WeightedList.getRandomOrThrow`: `nextInt(total)`, then the first whose weight covers it.
template <typename Value>
[[nodiscard]] const Value* pickWeighted(const std::vector<Value>& values, const std::vector<std::int32_t>& weights,
                                        random::RandomSource& random) {
    std::int32_t total = 0;
    for (const std::int32_t weight : weights) {
        total += weight;
    }
    if (total <= 0) {
        return nullptr;
    }
    std::int32_t selection = random.nextInt(total);
    for (std::size_t index = 0; index < values.size(); ++index) {
        selection -= weights[index];
        if (selection < 0) {
            return &values[index];
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<std::string> holderOf(const ScoreTarget& target, const LootContext& context) {
    if (!target.entity.has_value()) {
        return target.name;
    }
    return context.scoreHolder ? context.scoreHolder(*target.entity) : std::nullopt;
}

} // namespace

IntProvider IntProvider::constant(std::int32_t value) {
    IntProvider built;
    built.kind_ = Kind::Constant;
    built.value_ = value;
    return built;
}

IntProvider IntProvider::uniform(IntProvider min, IntProvider max) {
    return binary(Kind::Uniform, std::move(min), std::move(max));
}

IntProvider IntProvider::binomial(IntProvider n, FloatProvider p) {
    IntProvider built;
    built.kind_ = Kind::Binomial;
    built.parts_ = std::make_shared<const Parts>(Parts{.ints = {std::move(n)}, .floats = {std::move(p)}});
    return built;
}

IntProvider IntProvider::aggregate(Kind kind, std::vector<IntProvider> inputs) {
    IntProvider built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(Parts{.ints = std::move(inputs)});
    return built;
}

IntProvider IntProvider::binary(Kind kind, IntProvider left, IntProvider right) {
    IntProvider built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(Parts{.ints = {std::move(left), std::move(right)}});
    return built;
}

IntProvider IntProvider::unary(Kind kind, IntProvider input) {
    IntProvider built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(Parts{.ints = {std::move(input)}});
    return built;
}

IntProvider IntProvider::fromFloat(FloatProvider input) {
    IntProvider built;
    built.kind_ = Kind::FromFloat;
    built.parts_ = std::make_shared<const Parts>(Parts{.floats = {std::move(input)}});
    return built;
}

IntProvider IntProvider::conditional(LootCondition condition, IntProvider onTrue, IntProvider onFalse) {
    IntProvider built;
    built.kind_ = Kind::Conditional;
    built.parts_ = std::make_shared<const Parts>(
        Parts{.ints = {std::move(onTrue), std::move(onFalse)}, .conditions = {std::move(condition)}});
    return built;
}

IntProvider IntProvider::dispatcher(std::vector<std::pair<LootCondition, IntProvider>> cases, IntProvider fallback) {
    Parts parts;
    for (auto& [condition, value] : cases) {
        parts.conditions.push_back(std::move(condition));
        parts.ints.push_back(std::move(value));
    }
    parts.ints.push_back(std::move(fallback));
    IntProvider built;
    built.kind_ = Kind::Dispatcher;
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

IntProvider IntProvider::weightedList(std::vector<std::pair<IntProvider, std::int32_t>> distribution) {
    Parts parts;
    for (auto& [value, weight] : distribution) {
        parts.ints.push_back(std::move(value));
        parts.weights.push_back(weight);
    }
    IntProvider built;
    built.kind_ = Kind::WeightedList;
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

IntProvider IntProvider::score(ScoreTarget target, std::string objective, IntProvider fallback) {
    IntProvider built;
    built.kind_ = Kind::Score;
    built.parts_ = std::make_shared<const Parts>(
        Parts{.ints = {std::move(fallback)}, .target = std::move(target), .text = std::move(objective)});
    return built;
}

IntProvider IntProvider::storage(wire::common::Identifier id, std::string path, IntProvider fallback) {
    IntProvider built;
    built.kind_ = Kind::Storage;
    built.parts_ =
        std::make_shared<const Parts>(Parts{.ints = {std::move(fallback)}, .text = std::move(path), .id = id});
    return built;
}

IntProvider IntProvider::environmentAttribute(wire::common::Identifier attribute) {
    IntProvider built;
    built.kind_ = Kind::EnvironmentAttribute;
    built.parts_ = std::make_shared<const Parts>(Parts{.id = attribute});
    return built;
}

std::int32_t IntProvider::get(random::RandomSource& random, const LootContext& context) const {
    return tryGet(random, context).value_or(0);
}

std::optional<std::int32_t> IntProvider::tryGet(random::RandomSource& random, const LootContext& context) const {
    if (kind_ == Kind::Constant) {
        return value_;
    }
    const std::vector<IntProvider>& ints = parts_->ints;
    const auto both = [&]() -> std::optional<std::pair<std::int32_t, std::int32_t>> {
        const std::optional<std::int32_t> left = ints[0].tryGet(random, context);
        if (!left.has_value()) {
            return std::nullopt;
        }
        const std::optional<std::int32_t> right = ints[1].tryGet(random, context);
        if (!right.has_value()) {
            return std::nullopt;
        }
        return std::pair{*left, *right};
    };
    switch (kind_) {
    // parity: `Mth.nextInt`, inclusive and short-circuiting when the ends meet.
    case Kind::Uniform: {
        const auto bounds = both();
        if (!bounds.has_value()) {
            return std::nullopt;
        }
        const auto [least, most] = *bounds;
        if (least >= most) {
            return least;
        }
        // Java's int arithmetic wraps; a span past an int makes `nextInt` refuse, so nothing.
        const auto span =
            static_cast<std::int32_t>(static_cast<std::uint32_t>(most) - static_cast<std::uint32_t>(least) + 1U);
        return span <= 0 ? std::nullopt : std::optional{random.nextInt(span) + least};
    }
    case Kind::Binomial: {
        const std::optional<std::int32_t> tries = ints[0].tryGet(random, context);
        if (!tries.has_value()) {
            return std::nullopt;
        }
        const std::optional<float> chance = parts_->floats[0].tryGetFinite(random, context);
        if (!chance.has_value()) {
            return std::nullopt;
        }
        std::int32_t hits = 0;
        for (std::int32_t index = 0; index < *tries; ++index) {
            if (random.nextFloat() < *chance) {
                ++hits;
            }
        }
        return hits;
    }
    case Kind::Sum:
    case Kind::Product:
    case Kind::Average: {
        std::int64_t total = kind_ == Kind::Product ? 1 : 0;
        std::int64_t count = 0;
        for (const IntProvider& input : ints) {
            const std::optional<std::int32_t> value = input.tryGet(random, context);
            if (!value.has_value()) {
                return std::nullopt;
            }
            total = kind_ == Kind::Product ? wrappingMultiply(total, *value) : wrappingAdd(total, *value);
            ++count;
        }
        if (kind_ == Kind::Average) {
            if (count == 0) {
                return std::nullopt;
            }
            total /= count;
        }
        return longToInt(total);
    }
    case Kind::Minimum:
    case Kind::Maximum: {
        std::int32_t result = kind_ == Kind::Minimum ? kIntMax : kIntMin;
        for (const IntProvider& input : ints) {
            const std::optional<std::int32_t> value = input.tryGet(random, context);
            if (!value.has_value()) {
                return std::nullopt;
            }
            result = kind_ == Kind::Minimum ? std::min(result, *value) : std::max(result, *value);
        }
        return result;
    }
    case Kind::Difference: {
        const auto operands = both();
        return operands.has_value()
                   ? longToInt(static_cast<std::int64_t>(operands->first) - static_cast<std::int64_t>(operands->second))
                   : std::nullopt;
    }
    // parity: Java's `/` and `%` throw only for a zero divisor; `MIN_VALUE / -1` wraps to itself.
    case Kind::Quotient:
    case Kind::Modulus: {
        const auto operands = both();
        if (!operands.has_value() || operands->second == 0) {
            return std::nullopt;
        }
        if (operands->first == kIntMin && operands->second == -1) {
            return kind_ == Kind::Quotient ? kIntMin : 0;
        }
        return kind_ == Kind::Quotient ? operands->first / operands->second : operands->first % operands->second;
    }
    // parity: `Math.floorDivExact`, which throws on `MIN_VALUE / -1`, and `Math.floorMod`.
    case Kind::FloorQuotient:
    case Kind::FloorModulus: {
        const auto operands = both();
        if (!operands.has_value() || operands->second == 0) {
            return std::nullopt;
        }
        if (operands->first == kIntMin && operands->second == -1) {
            return kind_ == Kind::FloorQuotient ? std::nullopt : std::optional{0};
        }
        const std::int32_t quotient = floorDiv(operands->first, operands->second);
        return kind_ == Kind::FloorQuotient ? quotient : operands->first - (quotient * operands->second);
    }
    // parity: `Power`, where 0 to the 0 throws too.
    case Kind::Power: {
        const auto operands = both();
        if (!operands.has_value() || (operands->first == 0 && operands->second == 0)) {
            return std::nullopt;
        }
        return powExact(operands->first, operands->second);
    }
    // parity: `Math.negateExact` and `Math.absExact`, which throw only for `MIN_VALUE`.
    case Kind::Negate:
    case Kind::Absolute: {
        const std::optional<std::int32_t> value = ints[0].tryGet(random, context);
        if (!value.has_value() || *value == kIntMin) {
            return std::nullopt;
        }
        return kind_ == Kind::Negate ? -*value : std::abs(*value);
    }
    // parity: `floatToIntSafe`: not finite, or out of an int once truncated, throws.
    case Kind::FromFloat: {
        const std::optional<float> value = parts_->floats[0].tryGet(random, context);
        if (!value.has_value() || !std::isfinite(*value)) {
            return std::nullopt;
        }
        return longToInt(toLongLikeJava(static_cast<double>(*value)));
    }
    case Kind::Conditional:
        return ints[parts_->conditions[0].test(random, context) ? 0 : 1].tryGet(random, context);
    // parity: `DispatcherProvider.selectValue`, the first case whose condition passes.
    case Kind::Dispatcher:
        for (std::size_t index = 0; index < parts_->conditions.size(); ++index) {
            if (parts_->conditions[index].test(random, context)) {
                return ints[index].tryGet(random, context);
            }
        }
        return ints.back().tryGet(random, context);
    case Kind::WeightedList: {
        const IntProvider* picked = pickWeighted(ints, parts_->weights, random);
        return picked == nullptr ? std::nullopt : picked->tryGet(random, context);
    }
    case Kind::Score: {
        const std::optional<std::string> holder = holderOf(parts_->target, context);
        const std::optional<std::int32_t> held =
            holder.has_value() && context.score ? context.score(*holder, parts_->text) : std::nullopt;
        return held.has_value() ? held : ints[0].tryGet(random, context);
    }
    // parity: `Number.intValue` of what the storage holds, which truncates.
    case Kind::Storage: {
        const std::optional<double> held =
            context.storage ? context.storage(*parts_->id, parts_->text) : std::optional<double>{};
        return held.has_value() ? std::optional{toIntLikeJava(*held)} : ints[0].tryGet(random, context);
    }
    case Kind::EnvironmentAttribute: {
        const std::optional<double> held =
            context.environment ? context.environment(*parts_->id) : std::optional<double>{};
        return held.has_value() ? std::optional{toIntLikeJava(*held)} : std::nullopt;
    }
    default:
        return value_;
    }
}

FloatProvider FloatProvider::constant(float value) {
    FloatProvider built;
    built.kind_ = Kind::Constant;
    built.value_ = value;
    return built;
}

FloatProvider FloatProvider::uniform(FloatProvider min, FloatProvider max) {
    return binary(Kind::Uniform, std::move(min), std::move(max));
}

FloatProvider FloatProvider::aggregate(Kind kind, std::vector<FloatProvider> inputs) {
    FloatProvider built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(Parts{.floats = std::move(inputs)});
    return built;
}

FloatProvider FloatProvider::binary(Kind kind, FloatProvider left, FloatProvider right) {
    FloatProvider built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(Parts{.floats = {std::move(left), std::move(right)}});
    return built;
}

FloatProvider FloatProvider::unary(Kind kind, FloatProvider input) {
    FloatProvider built;
    built.kind_ = kind;
    built.parts_ = std::make_shared<const Parts>(Parts{.floats = {std::move(input)}});
    return built;
}

FloatProvider FloatProvider::fromInt(IntProvider input) {
    FloatProvider built;
    built.kind_ = Kind::FromInt;
    built.parts_ = std::make_shared<const Parts>(Parts{.ints = {std::move(input)}});
    return built;
}

FloatProvider FloatProvider::conditional(LootCondition condition, FloatProvider onTrue, FloatProvider onFalse) {
    FloatProvider built;
    built.kind_ = Kind::Conditional;
    built.parts_ = std::make_shared<const Parts>(
        Parts{.floats = {std::move(onTrue), std::move(onFalse)}, .conditions = {std::move(condition)}});
    return built;
}

FloatProvider FloatProvider::dispatcher(std::vector<std::pair<LootCondition, FloatProvider>> cases,
                                        FloatProvider fallback) {
    Parts parts;
    for (auto& [condition, value] : cases) {
        parts.conditions.push_back(std::move(condition));
        parts.floats.push_back(std::move(value));
    }
    parts.floats.push_back(std::move(fallback));
    FloatProvider built;
    built.kind_ = Kind::Dispatcher;
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

FloatProvider FloatProvider::weightedList(std::vector<std::pair<FloatProvider, std::int32_t>> distribution) {
    Parts parts;
    for (auto& [value, weight] : distribution) {
        parts.floats.push_back(std::move(value));
        parts.weights.push_back(weight);
    }
    FloatProvider built;
    built.kind_ = Kind::WeightedList;
    built.parts_ = std::make_shared<const Parts>(std::move(parts));
    return built;
}

FloatProvider FloatProvider::enchantmentLevel(LevelBasedValue amount) {
    FloatProvider built;
    built.kind_ = Kind::EnchantmentLevel;
    built.parts_ = std::make_shared<const Parts>(Parts{.level = amount});
    return built;
}

FloatProvider FloatProvider::storage(wire::common::Identifier id, std::string path, FloatProvider fallback) {
    FloatProvider built;
    built.kind_ = Kind::Storage;
    built.parts_ =
        std::make_shared<const Parts>(Parts{.floats = {std::move(fallback)}, .text = std::move(path), .id = id});
    return built;
}

FloatProvider FloatProvider::environmentAttribute(wire::common::Identifier attribute) {
    FloatProvider built;
    built.kind_ = Kind::EnvironmentAttribute;
    built.parts_ = std::make_shared<const Parts>(Parts{.id = attribute});
    return built;
}

float FloatProvider::get(random::RandomSource& random, const LootContext& context) const {
    const std::optional<float> value = tryGet(random, context);
    return value.has_value() && std::isfinite(*value) ? *value : 0.0F;
}

std::optional<float> FloatProvider::tryGetFinite(random::RandomSource& random, const LootContext& context) const {
    const std::optional<float> value = tryGet(random, context);
    return value.has_value() && std::isfinite(*value) ? value : std::nullopt;
}

std::optional<float> FloatProvider::tryGet(random::RandomSource& random, const LootContext& context) const {
    if (kind_ == Kind::Constant) {
        return value_;
    }
    const std::vector<FloatProvider>& floats = parts_->floats;
    const auto one = [&]() { return floats[0].tryGet(random, context); };
    const auto both = [&]() -> std::optional<std::pair<float, float>> {
        const std::optional<float> left = floats[0].tryGet(random, context);
        if (!left.has_value()) {
            return std::nullopt;
        }
        const std::optional<float> right = floats[1].tryGet(random, context);
        if (!right.has_value()) {
            return std::nullopt;
        }
        return std::pair{*left, *right};
    };
    switch (kind_) {
    // parity: `Mth.nextFloat`, `nextFloat() * (max - min) + min` unless the ends meet.
    case Kind::Uniform: {
        const auto bounds = both();
        if (!bounds.has_value()) {
            return std::nullopt;
        }
        const auto [least, most] = *bounds;
        return least >= most ? least : (random.nextFloat() * (most - least)) + least;
    }
    case Kind::Sum:
    case Kind::Product:
    case Kind::Average:
    case Kind::Length:
    case Kind::Minimum:
    case Kind::Maximum: {
        float result = kind_ == Kind::Product   ? 1.0F
                       : kind_ == Kind::Minimum ? std::numeric_limits<float>::max()
                       : kind_ == Kind::Maximum ? -std::numeric_limits<float>::max()
                                                : 0.0F;
        std::int32_t count = 0;
        for (const FloatProvider& input : floats) {
            const std::optional<float> value = input.tryGet(random, context);
            if (!value.has_value()) {
                return std::nullopt;
            }
            switch (kind_) {
            case Kind::Product:
                result *= *value;
                break;
            case Kind::Minimum:
                result = javaMin(result, *value);
                break;
            case Kind::Maximum:
                result = javaMax(result, *value);
                break;
            case Kind::Length: {
                const float squared = *value * *value;
                result += squared;
                break;
            }
            default:
                result += *value;
                break;
            }
            ++count;
        }
        if (kind_ == Kind::Average) {
            return result / static_cast<float>(count);
        }
        // parity: `Mth.sqrt(float)`, the double root narrowed.
        return kind_ == Kind::Length ? static_cast<float>(std::sqrt(static_cast<double>(result))) : result;
    }
    case Kind::Difference:
    case Kind::Quotient: {
        const auto operands = both();
        if (!operands.has_value()) {
            return std::nullopt;
        }
        return kind_ == Kind::Difference ? operands->first - operands->second : operands->first / operands->second;
    }
    // parity: `Modulus` reads the divisor first and answers NaN for zero, the dividend unread.
    case Kind::Modulus: {
        const std::optional<float> right = floats[1].tryGet(random, context);
        if (!right.has_value()) {
            return std::nullopt;
        }
        if (*right == 0.0F) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        const std::optional<float> left = floats[0].tryGet(random, context);
        return left.has_value() ? std::optional{std::fmod(*left, *right)} : std::nullopt;
    }
    case Kind::Power: {
        const auto operands = both();
        if (!operands.has_value()) {
            return std::nullopt;
        }
        if (operands->first == 0.0F && operands->second == 0.0F) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        return static_cast<float>(
            std::pow(static_cast<double>(operands->first), static_cast<double>(operands->second)));
    }
    case Kind::Negate:
    case Kind::Absolute:
    case Kind::Floor:
    case Kind::Ceiling:
    case Kind::Round:
    case Kind::Truncate:
    case Kind::SquareRoot:
    case Kind::Sine:
    case Kind::Cosine: {
        const std::optional<float> input = one();
        if (!input.has_value()) {
            return std::nullopt;
        }
        const float value = *input;
        switch (kind_) {
        case Kind::Negate:
            return -value;
        case Kind::Absolute:
            return std::fabs(value);
        case Kind::Floor:
            return static_cast<float>(std::floor(static_cast<double>(value)));
        // parity: `Mth.ceil(float)` goes through an int, so it saturates and NaN becomes 0.
        case Kind::Ceiling:
            return static_cast<float>(toIntLikeJava(std::ceil(static_cast<double>(value))));
        case Kind::Round:
            return static_cast<float>(roundFloat(value));
        case Kind::Truncate:
            return value > 0.0F ? static_cast<float>(std::floor(static_cast<double>(value)))
                                : static_cast<float>(std::ceil(static_cast<double>(value)));
        case Kind::SquareRoot:
            return static_cast<float>(std::sqrt(static_cast<double>(value)));
        // parity: `Mth.sin` and `Mth.cos`, the lookup table and not the platform's.
        case Kind::Sine:
            return wire::common::mth::sin(static_cast<double>(value));
        default:
            return wire::common::mth::cos(static_cast<double>(value));
        }
    }
    case Kind::FromInt: {
        const std::optional<std::int32_t> value = parts_->ints[0].tryGet(random, context);
        return value.has_value() ? std::optional{static_cast<float>(*value)} : std::nullopt;
    }
    case Kind::Conditional:
        return floats[parts_->conditions[0].test(random, context) ? 0 : 1].tryGet(random, context);
    case Kind::Dispatcher:
        for (std::size_t index = 0; index < parts_->conditions.size(); ++index) {
            if (parts_->conditions[index].test(random, context)) {
                return floats[index].tryGet(random, context);
            }
        }
        return floats.back().tryGet(random, context);
    case Kind::WeightedList: {
        const FloatProvider* picked = pickWeighted(floats, parts_->weights, random);
        return picked == nullptr ? std::nullopt : picked->tryGet(random, context);
    }
    // parity: `EnchantmentLevelProvider`, level 0 when the roll has none.
    case Kind::EnchantmentLevel:
        return parts_->level.at(context.enchantmentLevel.value_or(0));
    case Kind::Storage: {
        const std::optional<double> held =
            context.storage ? context.storage(*parts_->id, parts_->text) : std::optional<double>{};
        return held.has_value() ? std::optional{static_cast<float>(*held)} : floats[0].tryGet(random, context);
    }
    case Kind::EnvironmentAttribute: {
        const std::optional<double> held =
            context.environment ? context.environment(*parts_->id) : std::optional<double>{};
        return held.has_value() ? std::optional{static_cast<float>(*held)} : std::nullopt;
    }
    default:
        return value_;
    }
}

} // namespace basalt::server::world::loot
