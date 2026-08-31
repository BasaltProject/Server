#pragma once

#include <basalt/server/world/spline/CubicSpline.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world {
namespace noise {
class NormalNoise;
}

namespace densityfunction {

class DensityCache;

/// parity: `DensityFunction.FunctionContext`, the single-point variant.
///
/// The cache is what vanilla gets by rewriting the tree per chunk instead. Null means no chunk
/// scope, and every marker then evaluates straight through.
struct FunctionContext {
    std::int32_t blockX = 0;
    std::int32_t blockY = 0;
    std::int32_t blockZ = 0;
    DensityCache* cache = nullptr;

    /// Moves this context to another block, so a caller sampling a whole chunk reuses one instead of
    /// building one per block. Safe only because no node keeps a context past its own `compute`:
    /// they read the three coordinates and pass the same instance down.
    void moveTo(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
        blockX = x;
        blockY = y;
        blockZ = z;
    }
};

/// parity: `DensityFunction`, the expression tree vanilla's noise router builds terrain shape from.
///
/// Only single-point evaluation and the declared bounds are modelled. The bounds are not
/// decoration: several node types fold constants and short-circuit on them, so a wrong bound is a
/// silent terrain difference. `fillArray` (a bulk fast path), the tree-rewriting visitors and the
/// data-driven codec are not here.
class DensityFunction {
public:
    virtual ~DensityFunction() = default;
    DensityFunction(const DensityFunction&) = delete;
    DensityFunction& operator=(const DensityFunction&) = delete;
    DensityFunction(DensityFunction&&) = delete;
    DensityFunction& operator=(DensityFunction&&) = delete;

    [[nodiscard]] virtual double compute(FunctionContext& context) const = 0;
    [[nodiscard]] virtual double minValue() const = 0;
    [[nodiscard]] virtual double maxValue() const = 0;

    /// The value this node always takes, when it always takes one.
    ///
    /// parity: vanilla asks `instanceof Constant` where it folds one; this asks the node instead,
    /// which is the same question and does not need the type back at runtime.
    [[nodiscard]] virtual std::optional<double> constantValue() const { return std::nullopt; }

protected:
    DensityFunction() = default;
};

using Function = std::shared_ptr<const DensityFunction>;

/// parity: `DensityFunctions.Mapped.Type`.
enum class MappedType : std::uint8_t { Abs, Square, Cube, HalfNegative, QuarterNegative, Invert, Squeeze };

/// parity: `DensityFunctions.TwoArgumentSimpleFunction.Type`.
enum class Ap2Type : std::uint8_t { Add, Mul, Min, Max };

/// parity: `DensityFunctions.Marker.Type`.
///
/// The flat cache is not merely an optimisation: vanilla quantises the query to the quart grid and
/// samples at y zero, so honouring it changes values toward vanilla. The 2D and once caches are
/// exact memoisation. The cell cache is defined against vanilla's own cell iteration and stays
/// performance-only here, and blend density only marks where Anvil blending would apply.
enum class MarkerType : std::uint8_t { Interpolated, FlatCache, Cache2d, CacheOnce, CacheAllInCell, BlendDensity };

/// The unary transform itself, which the bounds derivation needs as well as the node.
[[nodiscard]] double transform(MappedType type, double input);

/// How many cache slots every marker built so far needs between them.
[[nodiscard]] std::size_t markerSlotCount();

/// The factories. parity: `DensityFunctions`' own static methods.
namespace functions {

[[nodiscard]] Function zero();
[[nodiscard]] Function constant(double value);
[[nodiscard]] Function add(Function a, Function b);
[[nodiscard]] Function mul(Function a, Function b);
[[nodiscard]] Function min(Function a, Function b);
[[nodiscard]] Function max(Function a, Function b);
[[nodiscard]] Function clamp(Function input, double minValue, double maxValue);
[[nodiscard]] Function abs(Function input);
[[nodiscard]] Function square(Function input);
[[nodiscard]] Function cube(Function input);
[[nodiscard]] Function halfNegative(Function input);
[[nodiscard]] Function quarterNegative(Function input);
[[nodiscard]] Function invert(Function input);
[[nodiscard]] Function squeeze(Function input);
[[nodiscard]] Function interpolated(Function input);
[[nodiscard]] Function flatCache(Function input);
[[nodiscard]] Function cache2d(Function input);
[[nodiscard]] Function cacheOnce(Function input);
[[nodiscard]] Function cacheAllInCell(Function input);
[[nodiscard]] Function blendDensity(Function input);
[[nodiscard]] Function blendAlpha();
[[nodiscard]] Function blendOffset();
[[nodiscard]] Function yClampedGradient(std::int32_t fromY, std::int32_t toY, double fromValue, double toValue);
[[nodiscard]] Function noise(std::shared_ptr<const noise::NormalNoise> noise, double xzScale = 1.0,
                             double yScale = 1.0);
[[nodiscard]] Function mappedNoise(std::shared_ptr<const noise::NormalNoise> noise, double xzScale, double yScale,
                                   double minTarget, double maxTarget);
[[nodiscard]] Function shiftedNoise2d(Function shiftX, Function shiftZ, double xzScale,
                                      std::shared_ptr<const noise::NormalNoise> noise);
[[nodiscard]] Function shiftedNoise(Function shiftX, Function shiftY, Function shiftZ, double xzScale, double yScale,
                                    std::shared_ptr<const noise::NormalNoise> noise);
[[nodiscard]] Function shiftA(std::shared_ptr<const noise::NormalNoise> noise);
[[nodiscard]] Function shiftB(std::shared_ptr<const noise::NormalNoise> noise);
[[nodiscard]] Function shift(std::shared_ptr<const noise::NormalNoise> noise);
[[nodiscard]] Function rangeChoice(Function input, double minInclusive, double maxExclusive, Function whenInRange,
                                   Function whenOutOfRange);
[[nodiscard]] Function intervalSelect(Function input, std::vector<double> thresholds, std::vector<Function> functions);
[[nodiscard]] Function spline(spline::SplineRef<FunctionContext> curve);

/// parity: `BlendedNoise`, the base 3D terrain noise every dimension samples one of.
///
/// Three legacy-seeded Perlin noises blended by how far the main one leans toward its own extremes.
[[nodiscard]] Function blendedNoise(basalt::server::random::RandomSource& source, double xzScale, double yScale,
                                    double xzFactor, double yFactor, double smearScaleMultiplier);

/// parity: `BlendedNoise.createUnseeded`, always seeded with xoroshiro zero: the noise routers
/// register this at bootstrap with a fixed placeholder seed, not the world seed.
[[nodiscard]] Function blendedNoiseUnseeded(double xzScale, double yScale, double xzFactor, double yFactor,
                                            double smearScaleMultiplier);

/// parity: `DensityFunctions.EndIslandDensityFunction`, the End's island shape.
[[nodiscard]] Function endIslands(std::int64_t seed);

/// parity: `DensityFunctions.FindTopSurface`, which scans downward for where the density turns
/// positive.
[[nodiscard]] Function findTopSurface(Function density, Function upperBound, std::int32_t lowerBound,
                                      std::int32_t cellHeight);

/// The three-function form, which special-cases a constant first argument.
[[nodiscard]] Function lerp(Function alpha, Function first, Function second);

/// The form the terrain slides actually call.
[[nodiscard]] Function lerp(Function factor, double first, Function second);

/// Wraps a density function as a spline's x axis.
[[nodiscard]] spline::BoundedFloatFunction<FunctionContext> splineCoordinate(Function function);

} // namespace functions

} // namespace densityfunction
} // namespace basalt::server::world
