#pragma once

#include <basalt/server/world/densityfunction/DensityVolume.hpp>
#include <basalt/server/world/noise/Interval.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world::noise {
class Noise;
class NormalNoise;
} // namespace basalt::server::world::noise

namespace basalt::server::world::densityfunction {

class SamplerContext;
class DensityFunction;
class DensitySampler;

using Function = std::shared_ptr<const DensityFunction>;
using Sampler = std::shared_ptr<const DensitySampler>;

/// parity: `DensityFunction.AXIS_*`, the axes a function depends on.
inline constexpr std::int32_t kAxisX = 1;
inline constexpr std::int32_t kAxisY = 2;
inline constexpr std::int32_t kAxisZ = 4;
inline constexpr std::int32_t kAllAxes = 7;

enum class Axis : std::uint8_t { X, Y, Z };

[[nodiscard]] constexpr std::int32_t axesFrom(Axis axis) noexcept {
    return axis == Axis::X ? kAxisX : (axis == Axis::Y ? kAxisY : kAxisZ);
}

/// parity: `DensityFunction.CompileContext`, what a function needs from the world to sample.
class CompileContext {
public:
    virtual ~CompileContext() = default;

    /// The sampler for a named noise, or the legacy-seeded one the Nether's climate asks for.
    [[nodiscard]] virtual std::shared_ptr<const noise::Noise> createNoise(const std::string& noiseId) = 0;

    [[nodiscard]] virtual random::RandomSource createRandom(std::string_view seedName) = 0;

    [[nodiscard]] virtual random::RandomSource createEndIslandRandom() = 0;
};

/// parity: `DfRewriteRule`.
class RewriteRule {
public:
    virtual ~RewriteRule() = default;
    [[nodiscard]] virtual Function rewrite(const Function& function) const = 0;
};

/// parity: `DensitySampler`, a compiled function. The point path and the volume path are both
/// part of the contract: vanilla uses each where it does, and they do not always round alike.
class DensitySampler {
public:
    virtual ~DensitySampler() = default;
    DensitySampler(const DensitySampler&) = delete;
    DensitySampler& operator=(const DensitySampler&) = delete;
    DensitySampler(DensitySampler&&) = delete;
    DensitySampler& operator=(DensitySampler&&) = delete;

    [[nodiscard]] virtual float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                            std::int32_t z) const = 0;

    virtual void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const = 0;

protected:
    DensitySampler() = default;
};

/// The node kinds, which rewrite rules and constant folding switch on.
enum class Kind : std::uint8_t {
    Constant,
    Reference,
    Simple,
    Noise,
    EndIslands,
    DistanceToPoint,
    Gradient,
    Shift,
    Unary,
    Binary,
    Spline,
    Lerp,
    Clamp,
    RangeChoice,
    IntervalSelect,
    Cache,
    PreparedCache,
    BlendDensity,
    Interpolated,
    Slice,
    FindTopSurface,
    BlendedNoise,
};

/// parity: `DensityFunction`, an immutable expression node. Structural equality matters: vanilla
/// shares one cache between every `cache` node whose input compares equal.
class DensityFunction {
public:
    virtual ~DensityFunction() = default;
    DensityFunction(const DensityFunction&) = delete;
    DensityFunction& operator=(const DensityFunction&) = delete;
    DensityFunction(DensityFunction&&) = delete;
    DensityFunction& operator=(DensityFunction&&) = delete;

    [[nodiscard]] virtual Kind kind() const noexcept = 0;
    [[nodiscard]] virtual noise::Interval range() const = 0;
    [[nodiscard]] virtual std::int32_t domainAxes() const = 0;
    [[nodiscard]] virtual Sampler compile(CompileContext& context) const = 0;

    /// `self` is this node's own handle, returned when no child changed.
    [[nodiscard]] virtual Function rewriteChildren(const Function& self, const RewriteRule& rule) const = 0;

    [[nodiscard]] virtual bool equals(const DensityFunction& other) const = 0;
    [[nodiscard]] virtual std::size_t hash() const = 0;

    /// The value of a `constant` node, and nothing for any other.
    [[nodiscard]] virtual std::optional<float> constantValue() const { return std::nullopt; }

protected:
    DensityFunction() = default;
};

[[nodiscard]] bool equal(const Function& left, const Function& right);

/// parity: `SimpleDensityFunction`.
enum class SimpleType : std::uint8_t { BlendAlpha, BlendOffset, Beardifier };

/// parity: `UnaryFunction.Type`.
enum class UnaryType : std::uint8_t {
    Abs,
    Square,
    Cube,
    Sqrt,
    HalfNegative,
    QuarterNegative,
    Reciprocal,
    Negate,
    Squeeze,
    Log,
    Sign
};

/// parity: `BinaryFunction.Type`.
enum class BinaryType : std::uint8_t { Add, Sub, Mul, Div, Min, Max };

/// parity: `TilingMode`.
enum class Tiling : std::uint8_t { ClampToEdge, Repeat, MirroredRepeat };

/// parity: `DistanceMetric`.
enum class DistanceMetric : std::uint8_t { Euclidean, EuclideanSquared, Manhattan, Chebyshev };

/// parity: `ShiftNoiseFunction`'s three shapes.
enum class ShiftType : std::uint8_t { ShiftA, ShiftB, Shift };

/// parity: `CubicSpline` over density coordinates, float throughout.
struct Spline {
    struct Point {
        float location;
        std::shared_ptr<const Spline> value;
        float derivative;
    };

    /// A constant spline has no coordinate and no points.
    std::optional<float> constant;
    Function coordinate;
    std::vector<Point> points;
};

namespace functions {

[[nodiscard]] Function zero();
[[nodiscard]] Function constant(float value);
[[nodiscard]] Function simple(SimpleType type);
[[nodiscard]] Function reference(std::string id, Function target);
[[nodiscard]] Function noise(std::string noiseId, std::shared_ptr<const noise::NormalNoise> parameters, double xzScale,
                             double yScale, Function shiftX, Function shiftY, Function shiftZ);
[[nodiscard]] Function endIslands();
[[nodiscard]] Function distanceToPoint(std::int32_t x, std::int32_t y, std::int32_t z, DistanceMetric metric);
[[nodiscard]] Function gradient(Axis axis, Tiling tiling, std::int32_t fromCoordinate, std::int32_t toCoordinate,
                                float fromValue, float toValue);
[[nodiscard]] Function shift(ShiftType type, std::string noiseId, std::shared_ptr<const noise::NormalNoise> parameters);
[[nodiscard]] Function unary(UnaryType type, Function input);
[[nodiscard]] Function binary(BinaryType type, Function left, Function right);
[[nodiscard]] Function spline(std::shared_ptr<const Spline> spline);
[[nodiscard]] Function lerp(Function alpha, Function first, Function second);
[[nodiscard]] Function clamp(Function input, float min, float max);
[[nodiscard]] Function rangeChoice(Function input, float minInclusive, float maxExclusive, Function whenInRange,
                                   Function whenOutOfRange);
[[nodiscard]] Function intervalSelect(Function input, std::vector<float> thresholds, std::vector<Function> functions);
[[nodiscard]] Function cache(Function input);
[[nodiscard]] Function blendDensity(Function input);
[[nodiscard]] Function interpolated(Function input, std::int32_t cellSizeXz, std::int32_t cellSizeY);
[[nodiscard]] Function slice(Axis axis, std::int32_t coordinate, Function input);
[[nodiscard]] Function findTopSurface(Function density, Function upperBound, std::int32_t lowerBound,
                                      std::int32_t cellHeight);
[[nodiscard]] Function blendedNoise(double xzScale, double yScale, double xzFactor, double yFactor,
                                    double smearScaleMultiplier);

[[nodiscard]] inline Function add(Function left, Function right) {
    return binary(BinaryType::Add, std::move(left), std::move(right));
}

[[nodiscard]] inline Function mul(Function left, Function right) {
    return binary(BinaryType::Mul, std::move(left), std::move(right));
}

/// parity: `DensityFunctions.yClampedGradient`.
[[nodiscard]] inline Function yClampedGradient(std::int32_t fromY, std::int32_t toY, float fromValue, float toValue) {
    return gradient(Axis::Y, Tiling::ClampToEdge, fromY, toY, fromValue, toValue);
}

/// parity: `BlendedNoise.compileSampler(RandomSource)`, for a caller seeding it by hand.
[[nodiscard]] Sampler compileBlendedNoise(random::RandomSource& random, double xzScale, double yScale, double xzFactor,
                                          double yFactor, double smearScaleMultiplier);

} // namespace functions

/// The target of a reference node, or the node itself for any other.
[[nodiscard]] const Function& inlineReference(const Function& function);

} // namespace basalt::server::world::densityfunction
