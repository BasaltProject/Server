#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::loot {

class LootCondition;
class FloatProvider;

/// Where a `score` provider reads its holder from. parity: `ScoreboardNameProvider`: one of the
struct ScoreTarget {
    std::optional<EntityTarget> entity;
    std::string name;
};

/// A whole number worked out from the roll. parity: `ContextIntProvider` and its 23 types.
class IntProvider {
public:
    enum class Kind : std::uint8_t {
        Constant,
        Uniform,
        Binomial,
        Sum,
        Product,
        Minimum,
        Maximum,
        Average,
        Difference,
        Quotient,
        FloorQuotient,
        Modulus,
        FloorModulus,
        Power,
        Negate,
        Absolute,
        FromFloat,
        Conditional,
        Dispatcher,
        WeightedList,
        Score,
        Storage,
        EnvironmentAttribute,
    };

    IntProvider() = default;

    [[nodiscard]] static IntProvider constant(std::int32_t value);
    [[nodiscard]] static IntProvider uniform(IntProvider min, IntProvider max);
    [[nodiscard]] static IntProvider binomial(IntProvider n, FloatProvider p);
    [[nodiscard]] static IntProvider aggregate(Kind kind, std::vector<IntProvider> inputs);
    [[nodiscard]] static IntProvider binary(Kind kind, IntProvider left, IntProvider right);
    [[nodiscard]] static IntProvider unary(Kind kind, IntProvider input);
    [[nodiscard]] static IntProvider fromFloat(FloatProvider input);
    [[nodiscard]] static IntProvider conditional(LootCondition condition, IntProvider onTrue, IntProvider onFalse);
    [[nodiscard]] static IntProvider dispatcher(std::vector<std::pair<LootCondition, IntProvider>> cases,
                                                IntProvider fallback);
    [[nodiscard]] static IntProvider weightedList(std::vector<std::pair<IntProvider, std::int32_t>> distribution);
    [[nodiscard]] static IntProvider score(ScoreTarget target, std::string objective, IntProvider fallback);
    [[nodiscard]] static IntProvider storage(wire::common::Identifier id, std::string path, IntProvider fallback);
    [[nodiscard]] static IntProvider environmentAttribute(wire::common::Identifier attribute);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    /// parity: `getInt`.
    [[nodiscard]] std::int32_t get(random::RandomSource& random, const LootContext& context) const;

    /// parity: `getIntUnsafe`.
    [[nodiscard]] std::optional<std::int32_t> tryGet(random::RandomSource& random, const LootContext& context) const;

private:
    struct Parts;

    Kind kind_ = Kind::Constant;
    std::int32_t value_ = 0;
    std::shared_ptr<const Parts> parts_;
};

/// A fraction worked out from the roll. parity: `ContextFloatProvider` and its 28 types.
class FloatProvider {
public:
    enum class Kind : std::uint8_t {
        Constant,
        Uniform,
        Sum,
        Product,
        Minimum,
        Maximum,
        Average,
        Length,
        Difference,
        Quotient,
        Modulus,
        Power,
        Negate,
        Absolute,
        Floor,
        Ceiling,
        Round,
        Truncate,
        SquareRoot,
        Sine,
        Cosine,
        FromInt,
        Conditional,
        Dispatcher,
        WeightedList,
        EnchantmentLevel,
        Storage,
        EnvironmentAttribute,
    };

    FloatProvider() = default;

    [[nodiscard]] static FloatProvider constant(float value);
    [[nodiscard]] static FloatProvider uniform(FloatProvider min, FloatProvider max);
    [[nodiscard]] static FloatProvider aggregate(Kind kind, std::vector<FloatProvider> inputs);
    [[nodiscard]] static FloatProvider binary(Kind kind, FloatProvider left, FloatProvider right);
    [[nodiscard]] static FloatProvider unary(Kind kind, FloatProvider input);
    [[nodiscard]] static FloatProvider fromInt(IntProvider input);
    [[nodiscard]] static FloatProvider conditional(LootCondition condition, FloatProvider onTrue,
                                                   FloatProvider onFalse);
    [[nodiscard]] static FloatProvider dispatcher(std::vector<std::pair<LootCondition, FloatProvider>> cases,
                                                  FloatProvider fallback);
    [[nodiscard]] static FloatProvider weightedList(std::vector<std::pair<FloatProvider, std::int32_t>> distribution);
    [[nodiscard]] static FloatProvider enchantmentLevel(LevelBasedValue amount);
    [[nodiscard]] static FloatProvider storage(wire::common::Identifier id, std::string path, FloatProvider fallback);
    [[nodiscard]] static FloatProvider environmentAttribute(wire::common::Identifier attribute);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    /// parity: `getFloat`: 0 for anything not finite or that would have thrown.
    [[nodiscard]] float get(random::RandomSource& random, const LootContext& context) const;

    /// parity: `getFloatUnsafe`.
    [[nodiscard]] std::optional<float> tryGet(random::RandomSource& random, const LootContext& context) const;

    /// parity: `getFloatOrThrow`, which also throws for a value that is not finite.
    [[nodiscard]] std::optional<float> tryGetFinite(random::RandomSource& random, const LootContext& context) const;

private:
    struct Parts;

    Kind kind_ = Kind::Constant;
    float value_ = 0.0F;
    std::shared_ptr<const Parts> parts_;
};

} // namespace basalt::server::world::loot
