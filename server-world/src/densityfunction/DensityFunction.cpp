#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/densityfunction/DensityCache.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/noise/PerlinNoise.hpp>
#include <basalt/server/world/noise/SimplexNoise.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace basalt::server::world::densityfunction {
namespace {

namespace mth = wire::common::mth;

/// parity: `DensityFunctions.Constant`, a fixed value that ignores position.
class Constant final : public DensityFunction {
public:
    explicit Constant(double value) : value_(value) {}

    [[nodiscard]] double compute(FunctionContext&) const override { return value_; }

    [[nodiscard]] double minValue() const override { return value_; }

    [[nodiscard]] double maxValue() const override { return value_; }

    [[nodiscard]] std::optional<double> constantValue() const override { return value_; }

private:
    double value_;
};

/// parity: `DensityFunctions.TwoArgumentSimpleFunction`. Bounds derived once, at construction.
class Ap2 final : public DensityFunction {
public:
    Ap2(Ap2Type type, Function first, Function second, double minValue, double maxValue)
        : type_(type), first_(std::move(first)), second_(std::move(second)), min_(minValue), max_(maxValue) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const double value = first_->compute(context);
        switch (type_) {
        case Ap2Type::Add:
            return value + second_->compute(context);
        case Ap2Type::Mul:
            // Vanilla short-circuits an exact zero, and the multiply that follows is skipped with it.
            return value == 0.0 ? 0.0 : value * second_->compute(context);
        case Ap2Type::Min:
            return value < second_->minValue() ? value : std::min(value, second_->compute(context));
        default:
            return value > second_->maxValue() ? value : std::max(value, second_->compute(context));
        }
    }

    [[nodiscard]] double minValue() const override { return min_; }

    [[nodiscard]] double maxValue() const override { return max_; }

private:
    Ap2Type type_;
    Function first_;
    Function second_;
    double min_;
    double max_;
};

[[nodiscard]] Function makeAp2(Ap2Type type, Function first, Function second) {
    const double min1 = first->minValue();
    const double min2 = second->minValue();
    const double max1 = first->maxValue();
    const double max2 = second->maxValue();
    double minValue = 0.0;
    double maxValue = 0.0;
    switch (type) {
    case Ap2Type::Add:
        minValue = min1 + min2;
        maxValue = max1 + max2;
        break;
    case Ap2Type::Mul:
        if (min1 > 0.0 && min2 > 0.0) {
            minValue = min1 * min2;
            maxValue = max1 * max2;
        } else if (max1 < 0.0 && max2 < 0.0) {
            minValue = max1 * max2;
            maxValue = min1 * min2;
        } else {
            minValue = std::min(min1 * max2, max1 * min2);
            maxValue = std::max(min1 * min2, max1 * max2);
        }
        break;
    case Ap2Type::Min:
        minValue = std::min(min1, min2);
        maxValue = std::min(max1, max2);
        break;
    default:
        minValue = std::max(min1, min2);
        maxValue = std::max(max1, max2);
        break;
    }
    return std::make_shared<Ap2>(type, std::move(first), std::move(second), minValue, maxValue);
}

/// parity: `DensityFunctions.Mapped`, a unary transform whose bounds come from its input's.
class Mapped final : public DensityFunction {
public:
    Mapped(MappedType type, Function input, double minValue, double maxValue)
        : type_(type), input_(std::move(input)), min_(minValue), max_(maxValue) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        return transform(type_, input_->compute(context));
    }

    [[nodiscard]] double minValue() const override { return min_; }

    [[nodiscard]] double maxValue() const override { return max_; }

private:
    MappedType type_;
    Function input_;
    double min_;
    double max_;
};

[[nodiscard]] Function makeMapped(MappedType type, Function input) {
    const double minValue = input->minValue();
    const double maxValue = input->maxValue();
    const double minImage = transform(type, minValue);
    const double maxImage = transform(type, maxValue);
    if (type == MappedType::Invert) {
        // An inverted range straddling zero is unbounded on both sides.
        if (minValue < 0.0 && maxValue > 0.0) {
            return std::make_shared<Mapped>(type, std::move(input), -std::numeric_limits<double>::infinity(),
                                            std::numeric_limits<double>::infinity());
        }
        return std::make_shared<Mapped>(type, std::move(input), maxImage, minImage);
    }
    if (type == MappedType::Abs || type == MappedType::Square) {
        return std::make_shared<Mapped>(type, std::move(input), std::max(0.0, minValue), std::max(minImage, maxImage));
    }
    return std::make_shared<Mapped>(type, std::move(input), minImage, maxImage);
}

/// parity: `DensityFunctions.Clamp`.
class Clamp final : public DensityFunction {
public:
    Clamp(Function input, double minValue, double maxValue)
        : input_(std::move(input)), min_(minValue), max_(maxValue) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        return mth::clamp(input_->compute(context), min_, max_);
    }

    [[nodiscard]] double minValue() const override { return min_; }

    [[nodiscard]] double maxValue() const override { return max_; }

private:
    Function input_;
    double min_;
    double max_;
};

/// parity: `DensityFunctions.YClampedGradient`, a linear ramp over a block-Y span.
class YClampedGradient final : public DensityFunction {
public:
    YClampedGradient(std::int32_t fromY, std::int32_t toY, double fromValue, double toValue)
        : fromY_(fromY), toY_(toY), fromValue_(fromValue), toValue_(toValue) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        return mth::clampedMap(static_cast<double>(context.blockY), static_cast<double>(fromY_),
                               static_cast<double>(toY_), fromValue_, toValue_);
    }

    [[nodiscard]] double minValue() const override { return std::min(fromValue_, toValue_); }

    [[nodiscard]] double maxValue() const override { return std::max(fromValue_, toValue_); }

private:
    std::int32_t fromY_;
    std::int32_t toY_;
    double fromValue_;
    double toValue_;
};

/// parity: `DensityFunctions.Noise`.
class Noise final : public DensityFunction {
public:
    Noise(std::shared_ptr<const noise::NormalNoise> noise, double xzScale, double yScale)
        : noise_(std::move(noise)), xzScale_(xzScale), yScale_(yScale) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        return noise_->getValue(context.blockX * xzScale_, context.blockY * yScale_, context.blockZ * xzScale_);
    }

    [[nodiscard]] double minValue() const override { return -maxValue(); }

    [[nodiscard]] double maxValue() const override { return noise_->maxValue(); }

private:
    std::shared_ptr<const noise::NormalNoise> noise_;
    double xzScale_;
    double yScale_;
};

/// parity: `DensityFunctions.ShiftNoise` and its three shapes: a quarter-scaled, quadrupled sample
/// used to warp coordinates before they feed another function.
enum class ShiftKind : std::uint8_t { Shift, ShiftA, ShiftB };

class ShiftNoise final : public DensityFunction {
public:
    ShiftNoise(ShiftKind kind, std::shared_ptr<const noise::NormalNoise> noise)
        : kind_(kind), noise_(std::move(noise)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const auto x = static_cast<double>(context.blockX);
        const auto y = static_cast<double>(context.blockY);
        const auto z = static_cast<double>(context.blockZ);
        switch (kind_) {
        case ShiftKind::Shift:
            return sample(x, y, z);
        case ShiftKind::ShiftA:
            return sample(x, 0.0, z);
        default:
            return sample(z, x, 0.0);
        }
    }

    [[nodiscard]] double minValue() const override { return -maxValue(); }

    [[nodiscard]] double maxValue() const override { return noise_->maxValue() * 4.0; }

private:
    [[nodiscard]] double sample(double x, double y, double z) const {
        return noise_->getValue(x * 0.25, y * 0.25, z * 0.25) * 4.0;
    }

    ShiftKind kind_;
    std::shared_ptr<const noise::NormalNoise> noise_;
};

/// parity: `DensityFunctions.ShiftedNoise`.
class ShiftedNoise final : public DensityFunction {
public:
    ShiftedNoise(Function shiftX, Function shiftY, Function shiftZ, double xzScale, double yScale,
                 std::shared_ptr<const noise::NormalNoise> noise)
        : shiftX_(std::move(shiftX)), shiftY_(std::move(shiftY)), shiftZ_(std::move(shiftZ)), xzScale_(xzScale),
          yScale_(yScale), noise_(std::move(noise)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const double x = context.blockX * xzScale_ + shiftX_->compute(context);
        const double y = context.blockY * yScale_ + shiftY_->compute(context);
        const double z = context.blockZ * xzScale_ + shiftZ_->compute(context);
        return noise_->getValue(x, y, z);
    }

    [[nodiscard]] double minValue() const override { return -maxValue(); }

    [[nodiscard]] double maxValue() const override { return noise_->maxValue(); }

private:
    Function shiftX_;
    Function shiftY_;
    Function shiftZ_;
    double xzScale_;
    double yScale_;
    std::shared_ptr<const noise::NormalNoise> noise_;
};

/// parity: `DensityFunctions.RangeChoice`.
class RangeChoice final : public DensityFunction {
public:
    RangeChoice(Function input, double minInclusive, double maxExclusive, Function whenInRange, Function whenOutOfRange)
        : input_(std::move(input)), minInclusive_(minInclusive), maxExclusive_(maxExclusive),
          whenInRange_(std::move(whenInRange)), whenOutOfRange_(std::move(whenOutOfRange)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const double value = input_->compute(context);
        return value >= minInclusive_ && value < maxExclusive_ ? whenInRange_->compute(context)
                                                               : whenOutOfRange_->compute(context);
    }

    [[nodiscard]] double minValue() const override {
        return std::min(whenInRange_->minValue(), whenOutOfRange_->minValue());
    }

    [[nodiscard]] double maxValue() const override {
        return std::max(whenInRange_->maxValue(), whenOutOfRange_->maxValue());
    }

private:
    Function input_;
    double minInclusive_;
    double maxExclusive_;
    Function whenInRange_;
    Function whenOutOfRange_;
};

/// parity: `DensityFunctions.IntervalSelect`: the first threshold the input is strictly below picks
/// that bucket, falling through to the last function.
class IntervalSelect final : public DensityFunction {
public:
    IntervalSelect(Function input, std::vector<double> thresholds, std::vector<Function> functions)
        : input_(std::move(input)), thresholds_(std::move(thresholds)), functions_(std::move(functions)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const double value = input_->compute(context);
        for (std::size_t i = 0; i < thresholds_.size(); ++i) {
            if (value < thresholds_[i]) {
                return functions_[i]->compute(context);
            }
        }
        return functions_.back()->compute(context);
    }

    [[nodiscard]] double minValue() const override {
        double lowest = std::numeric_limits<double>::infinity();
        for (const Function& one : functions_) {
            lowest = std::min(lowest, one->minValue());
        }
        return lowest;
    }

    [[nodiscard]] double maxValue() const override {
        double highest = -std::numeric_limits<double>::infinity();
        for (const Function& one : functions_) {
            highest = std::max(highest, one->maxValue());
        }
        return highest;
    }

private:
    Function input_;
    std::vector<double> thresholds_;
    std::vector<Function> functions_;
};

/// parity: `DensityFunctions.BlendAlpha` and `BlendOffset`.
///
/// Always one and always zero: real vanilla worlds override them near old-format chunk boundaries
/// during terrain blending, which this world format does not need. Kept as leaves so a tree that
/// names them still evaluates.
class Fixed final : public DensityFunction {
public:
    explicit Fixed(double value) : value_(value) {}

    [[nodiscard]] double compute(FunctionContext&) const override { return value_; }

    [[nodiscard]] double minValue() const override { return value_; }

    [[nodiscard]] double maxValue() const override { return value_; }

private:
    double value_;
};

/// parity: `DensityFunctions.Marker`.
class Marker final : public DensityFunction {
public:
    Marker(MarkerType type, Function wrapped, std::size_t slot)
        : type_(type), wrapped_(std::move(wrapped)), slot_(slot) {}

    [[nodiscard]] double compute(FunctionContext& context) const override;

    [[nodiscard]] double minValue() const override {
        return type_ == MarkerType::BlendDensity ? -std::numeric_limits<double>::infinity() : wrapped_->minValue();
    }

    [[nodiscard]] double maxValue() const override {
        return type_ == MarkerType::BlendDensity ? std::numeric_limits<double>::infinity() : wrapped_->maxValue();
    }

private:
    MarkerType type_;
    Function wrapped_;
    std::size_t slot_;
};

/// Slots are handed out for the life of the process, so a cache sized from the count covers every
/// marker any tree holds.
std::atomic<std::size_t> nextSlot{0};

[[nodiscard]] Function makeMarker(MarkerType type, Function wrapped) {
    return std::make_shared<Marker>(type, std::move(wrapped), nextSlot++);
}

/// parity: `DensityFunctions.Spline`.
class SplineFunction final : public DensityFunction {
public:
    explicit SplineFunction(spline::SplineRef<FunctionContext> curve) : curve_(std::move(curve)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        return static_cast<double>(curve_->sample(context));
    }

    [[nodiscard]] double minValue() const override { return static_cast<double>(curve_->minValue()); }

    [[nodiscard]] double maxValue() const override { return static_cast<double>(curve_->maxValue()); }

private:
    spline::SplineRef<FunctionContext> curve_;
};

/// parity: `BlendedNoise`. The three noises are seeded off one shared source, in this order.
class BlendedNoise final : public DensityFunction {
public:
    BlendedNoise(random::RandomSource& source, double xzScale, double yScale, double xzFactor, double yFactor,
                 double smearScaleMultiplier)
        : minLimit_(makeOctaves(source, 15)), maxLimit_(makeOctaves(source, 15)), main_(makeOctaves(source, 7)),
          xzFactor_(xzFactor), yFactor_(yFactor), smearScaleMultiplier_(smearScaleMultiplier),
          xzMultiplier_(kXzYzScale * xzScale), yMultiplier_(kXzYzScale * yScale),
          maxValue_(minLimit_.maxBrokenValue(kXzYzScale * yScale)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const double limitX = context.blockX * xzMultiplier_;
        const double limitY = context.blockY * yMultiplier_;
        const double limitZ = context.blockZ * xzMultiplier_;
        const double mainX = limitX / xzFactor_;
        const double mainY = limitY / yFactor_;
        const double mainZ = limitZ / xzFactor_;
        const double limitSmear = yMultiplier_ * smearScaleMultiplier_;
        const double mainSmear = limitSmear / yFactor_;

        double mainNoiseValue = 0.0;
        double pow = 1.0;
        for (std::size_t i = 0; i < 8; ++i) {
            if (const noise::ImprovedNoise* octave = main_.octaveNoise(i)) {
                mainNoiseValue +=
                    octave->noise(noise::PerlinNoise::wrap(mainX * pow), noise::PerlinNoise::wrap(mainY * pow),
                                  noise::PerlinNoise::wrap(mainZ * pow), mainSmear * pow, mainY * pow) /
                    pow;
            }
            pow /= 2.0;
        }

        const double factor = (mainNoiseValue / 10.0 + 1.0) / 2.0;
        const bool isMax = factor >= 1.0;
        const bool isMin = factor <= 0.0;
        double blendMin = 0.0;
        double blendMax = 0.0;
        pow = 1.0;
        for (std::size_t i = 0; i < 16; ++i) {
            const double wx = noise::PerlinNoise::wrap(limitX * pow);
            const double wy = noise::PerlinNoise::wrap(limitY * pow);
            const double wz = noise::PerlinNoise::wrap(limitZ * pow);
            const double yScalePow = limitSmear * pow;
            if (!isMax) {
                if (const noise::ImprovedNoise* octave = minLimit_.octaveNoise(i)) {
                    blendMin += octave->noise(wx, wy, wz, yScalePow, limitY * pow) / pow;
                }
            }
            if (!isMin) {
                if (const noise::ImprovedNoise* octave = maxLimit_.octaveNoise(i)) {
                    blendMax += octave->noise(wx, wy, wz, yScalePow, limitY * pow) / pow;
                }
            }
            pow /= 2.0;
        }
        return mth::clampedLerp(factor, blendMin / 512.0, blendMax / 512.0) / 128.0;
    }

    [[nodiscard]] double minValue() const override { return -maxValue_; }

    [[nodiscard]] double maxValue() const override { return maxValue_; }

private:
    static constexpr double kXzYzScale = 684.412;

    /// Octaves from `-count` up to zero, which is how vanilla names them.
    [[nodiscard]] static noise::PerlinNoise makeOctaves(random::RandomSource& source, int count) {
        std::vector<std::int32_t> octaves;
        octaves.reserve(static_cast<std::size_t>(count) + 1);
        for (int octave = -count; octave <= 0; ++octave) {
            octaves.push_back(octave);
        }
        return noise::PerlinNoise::createLegacyForBlendedNoise(source, octaves);
    }

    noise::PerlinNoise minLimit_;
    noise::PerlinNoise maxLimit_;
    noise::PerlinNoise main_;
    double xzFactor_;
    double yFactor_;
    double smearScaleMultiplier_;
    double xzMultiplier_;
    double yMultiplier_;
    double maxValue_;
};

/// parity: `DensityFunctions.EndIslandDensityFunction`.
///
/// Kept in float throughout rather than widened to double, to match vanilla's arithmetic bit for
/// bit: this algorithm's rounding is part of its observable output, not an implementation detail.
class EndIslands final : public DensityFunction {
public:
    explicit EndIslands(std::int64_t seed) : noise_(makeNoise(seed)) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        return (static_cast<double>(heightValue(context.blockX / 8, context.blockZ / 8)) - 8.0) / 128.0;
    }

    [[nodiscard]] double minValue() const override { return -0.84375; }

    [[nodiscard]] double maxValue() const override { return 0.5625; }

private:
    /// parity: the fixed skip vanilla makes before building the noise, part of the RNG consumption
    /// order the seed depends on.
    static constexpr int kSkippedRandomCalls = 17292;

    [[nodiscard]] static noise::SimplexNoise makeNoise(std::int64_t seed) {
        random::RandomSource source = random::RandomSource::legacy(seed);
        source.consumeCount(kSkippedRandomCalls);
        return noise::SimplexNoise{source};
    }

    [[nodiscard]] float heightValue(std::int32_t sectionX, std::int32_t sectionZ) const {
        const std::int32_t chunkX = sectionX / 2;
        const std::int32_t chunkZ = sectionZ / 2;
        const std::int32_t subSectionX = sectionX % 2;
        const std::int32_t subSectionZ = sectionZ % 2;
        float doffs = 100.0F - std::sqrt(static_cast<float>(sectionX * sectionX + sectionZ * sectionZ)) * 8.0F;
        doffs = mth::clamp(doffs, -100.0F, 80.0F);
        for (std::int32_t xo = -12; xo <= 12; ++xo) {
            for (std::int32_t zo = -12; zo <= 12; ++zo) {
                const auto totalChunkX = static_cast<std::int64_t>(chunkX + xo);
                const auto totalChunkZ = static_cast<std::int64_t>(chunkZ + zo);
                if (totalChunkX * totalChunkX + totalChunkZ * totalChunkZ <= 4096 ||
                    noise_.getValue(static_cast<double>(totalChunkX), static_cast<double>(totalChunkZ)) >= -0.9) {
                    continue;
                }
                const float islandSize = std::fmod(std::abs(static_cast<float>(totalChunkX)) * 3439.0F +
                                                       std::abs(static_cast<float>(totalChunkZ)) * 147.0F,
                                                   13.0F) +
                                         9.0F;
                const auto xd = static_cast<float>(subSectionX - xo * 2);
                const auto zd = static_cast<float>(subSectionZ - zo * 2);
                float newDoffs = 100.0F - std::sqrt(xd * xd + zd * zd) * islandSize;
                newDoffs = mth::clamp(newDoffs, -100.0F, 80.0F);
                doffs = std::max(doffs, newDoffs);
            }
        }
        return doffs;
    }

    noise::SimplexNoise noise_;
};

/// parity: `DensityFunctions.FindTopSurface`.
class FindTopSurface final : public DensityFunction {
public:
    FindTopSurface(Function density, Function upperBound, std::int32_t lowerBound, std::int32_t cellHeight)
        : density_(std::move(density)), upperBound_(std::move(upperBound)), lowerBound_(lowerBound),
          cellHeight_(cellHeight) {}

    [[nodiscard]] double compute(FunctionContext& context) const override {
        const std::int32_t topY = mth::floor(upperBound_->compute(context) / cellHeight_) * cellHeight_;
        if (topY <= lowerBound_) {
            return lowerBound_;
        }
        for (std::int32_t blockY = topY; blockY >= lowerBound_; blockY -= cellHeight_) {
            FunctionContext at{context.blockX, blockY, context.blockZ, context.cache};
            if (density_->compute(at) > 0.0) {
                return blockY;
            }
        }
        return lowerBound_;
    }

    [[nodiscard]] double minValue() const override { return lowerBound_; }

    [[nodiscard]] double maxValue() const override {
        return std::max(static_cast<double>(lowerBound_), upperBound_->maxValue());
    }

private:
    Function density_;
    Function upperBound_;
    std::int32_t lowerBound_;
    std::int32_t cellHeight_;
};

} // namespace

double transform(MappedType type, double input) {
    switch (type) {
    case MappedType::Abs:
        return std::abs(input);
    case MappedType::Square:
        return input * input;
    case MappedType::Cube:
        return input * input * input;
    case MappedType::HalfNegative:
        return input > 0.0 ? input : input * 0.5;
    case MappedType::QuarterNegative:
        return input > 0.0 ? input : input * 0.25;
    case MappedType::Invert:
        return 1.0 / input;
    default:
        // parity: Squeeze, which folds the range into a gentler curve.
        const double clamped = mth::clamp(input, -1.0, 1.0);
        return clamped / 2.0 - clamped * clamped * clamped / 24.0;
    }
}

std::size_t markerSlotCount() { return nextSlot.load(); }

double Marker::compute(FunctionContext& context) const {
    if (context.cache == nullptr) {
        return wrapped_->compute(context);
    }
    switch (type_) {
    case MarkerType::Interpolated:
        return context.cache->interpolated(slot_, context, *wrapped_);
    case MarkerType::FlatCache:
        return context.cache->flatCache(slot_, context, *wrapped_);
    case MarkerType::Cache2d:
        return context.cache->cache2d(slot_, context, *wrapped_);
    case MarkerType::CacheOnce:
        return context.cache->cacheOnce(slot_, context, *wrapped_);
    default:
        return wrapped_->compute(context);
    }
}

namespace functions {

Function zero() { return constant(0.0); }

Function constant(double value) { return std::make_shared<Constant>(value); }

Function add(Function a, Function b) { return makeAp2(Ap2Type::Add, std::move(a), std::move(b)); }

Function mul(Function a, Function b) { return makeAp2(Ap2Type::Mul, std::move(a), std::move(b)); }

Function min(Function a, Function b) { return makeAp2(Ap2Type::Min, std::move(a), std::move(b)); }

Function max(Function a, Function b) { return makeAp2(Ap2Type::Max, std::move(a), std::move(b)); }

Function clamp(Function input, double minValue, double maxValue) {
    return std::make_shared<Clamp>(std::move(input), minValue, maxValue);
}

Function abs(Function input) { return makeMapped(MappedType::Abs, std::move(input)); }

Function square(Function input) { return makeMapped(MappedType::Square, std::move(input)); }

Function cube(Function input) { return makeMapped(MappedType::Cube, std::move(input)); }

Function halfNegative(Function input) { return makeMapped(MappedType::HalfNegative, std::move(input)); }

Function quarterNegative(Function input) { return makeMapped(MappedType::QuarterNegative, std::move(input)); }

Function invert(Function input) { return makeMapped(MappedType::Invert, std::move(input)); }

Function squeeze(Function input) { return makeMapped(MappedType::Squeeze, std::move(input)); }

Function interpolated(Function input) { return makeMarker(MarkerType::Interpolated, std::move(input)); }

Function flatCache(Function input) { return makeMarker(MarkerType::FlatCache, std::move(input)); }

Function cache2d(Function input) { return makeMarker(MarkerType::Cache2d, std::move(input)); }

Function cacheOnce(Function input) { return makeMarker(MarkerType::CacheOnce, std::move(input)); }

Function cacheAllInCell(Function input) { return makeMarker(MarkerType::CacheAllInCell, std::move(input)); }

Function blendDensity(Function input) { return makeMarker(MarkerType::BlendDensity, std::move(input)); }

Function blendAlpha() { return std::make_shared<Fixed>(1.0); }

Function blendOffset() { return std::make_shared<Fixed>(0.0); }

Function yClampedGradient(std::int32_t fromY, std::int32_t toY, double fromValue, double toValue) {
    return std::make_shared<YClampedGradient>(fromY, toY, fromValue, toValue);
}

Function noise(std::shared_ptr<const noise::NormalNoise> noise, double xzScale, double yScale) {
    return std::make_shared<Noise>(std::move(noise), xzScale, yScale);
}

Function mappedNoise(std::shared_ptr<const noise::NormalNoise> source, double xzScale, double yScale, double minTarget,
                     double maxTarget) {
    // The unit range mapped onto the target one, which is an add of a mul.
    const double middle = (minTarget + maxTarget) * 0.5;
    const double factor = (maxTarget - minTarget) * 0.5;
    return add(constant(middle), mul(constant(factor), noise(std::move(source), xzScale, yScale)));
}

Function shiftedNoise(Function shiftX, Function shiftY, Function shiftZ, double xzScale, double yScale,
                      std::shared_ptr<const noise::NormalNoise> source) {
    return std::make_shared<ShiftedNoise>(std::move(shiftX), std::move(shiftY), std::move(shiftZ), xzScale, yScale,
                                          std::move(source));
}

Function shiftedNoise2d(Function shiftX, Function shiftZ, double xzScale,
                        std::shared_ptr<const noise::NormalNoise> source) {
    return shiftedNoise(std::move(shiftX), constant(0.0), std::move(shiftZ), xzScale, 0.0, std::move(source));
}

Function shiftA(std::shared_ptr<const noise::NormalNoise> source) {
    return std::make_shared<ShiftNoise>(ShiftKind::ShiftA, std::move(source));
}

Function shiftB(std::shared_ptr<const noise::NormalNoise> source) {
    return std::make_shared<ShiftNoise>(ShiftKind::ShiftB, std::move(source));
}

Function shift(std::shared_ptr<const noise::NormalNoise> source) {
    return std::make_shared<ShiftNoise>(ShiftKind::Shift, std::move(source));
}

Function rangeChoice(Function input, double minInclusive, double maxExclusive, Function whenInRange,
                     Function whenOutOfRange) {
    return std::make_shared<RangeChoice>(std::move(input), minInclusive, maxExclusive, std::move(whenInRange),
                                         std::move(whenOutOfRange));
}

Function intervalSelect(Function input, std::vector<double> thresholds, std::vector<Function> functions) {
    if (thresholds.size() + 1 != functions.size()) {
        throw std::invalid_argument{"an interval select needs one more function than it has thresholds"};
    }
    for (std::size_t i = 1; i < thresholds.size(); ++i) {
        if (thresholds[i - 1] > thresholds[i]) {
            throw std::invalid_argument{"an interval select's thresholds must ascend"};
        }
    }
    return std::make_shared<IntervalSelect>(std::move(input), std::move(thresholds), std::move(functions));
}

Function spline(spline::SplineRef<FunctionContext> curve) { return std::make_shared<SplineFunction>(std::move(curve)); }

Function lerp(Function alpha, Function first, Function second) {
    if (const std::optional<double> fixed = first->constantValue()) {
        return lerp(std::move(alpha), *fixed, std::move(second));
    }
    const Function alphaCached = cacheOnce(std::move(alpha));
    const Function oneMinusAlpha = add(mul(alphaCached, constant(-1.0)), constant(1.0));
    return add(mul(std::move(first), oneMinusAlpha), mul(std::move(second), alphaCached));
}

Function lerp(Function factor, double first, Function second) {
    return add(mul(std::move(factor), add(std::move(second), constant(-first))), constant(first));
}

Function blendedNoise(random::RandomSource& source, double xzScale, double yScale, double xzFactor, double yFactor,
                      double smearScaleMultiplier) {
    return std::make_shared<BlendedNoise>(source, xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier);
}

Function blendedNoiseUnseeded(double xzScale, double yScale, double xzFactor, double yFactor,
                              double smearScaleMultiplier) {
    random::RandomSource source = random::RandomSource::xoroshiro(0);
    return blendedNoise(source, xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier);
}

Function endIslands(std::int64_t seed) { return std::make_shared<EndIslands>(seed); }

Function findTopSurface(Function density, Function upperBound, std::int32_t lowerBound, std::int32_t cellHeight) {
    return std::make_shared<FindTopSurface>(std::move(density), std::move(upperBound), lowerBound, cellHeight);
}

spline::BoundedFloatFunction<FunctionContext> splineCoordinate(Function function) {
    const double minValue = function->minValue();
    const double maxValue = function->maxValue();
    return {[function = std::move(function)](const FunctionContext& context) {
                // The spline samples a const context; the tree below wants a mutable one, and no
                // node keeps it past its own compute.
                return static_cast<float>(function->compute(const_cast<FunctionContext&>(context)));
            },
            static_cast<float>(minValue), static_cast<float>(maxValue)};
}

} // namespace functions

} // namespace basalt::server::world::densityfunction
