#include "Nodes.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>
#include <basalt/server/world/noise/Noise.hpp>

#include <bit>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace basalt::server::world::densityfunction::nodes {
namespace {

namespace jm = noise::javaMath;

class ConstantSampler final : public DensitySampler {
public:
    explicit ConstantSampler(float value) : value_(value) {}

    [[nodiscard]] float sampleValue(SamplerContext&, std::int32_t, std::int32_t, std::int32_t) const override {
        return value_;
    }

    void sampleVolume(SamplerContext&, DensityBuffer& output, const DensityVolume&) const override {
        output.fill(value_);
    }

private:
    float value_;
};

/// parity: the beardifier's `ContextBoundSampler`, falling back to zero without a structure.
class BeardifierSampler final : public DensitySampler {
public:
    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const DensitySampler* bound = context.beardifier();
        return bound == nullptr ? 0.0F : bound->sampleValue(context, x, y, z);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        const DensitySampler* bound = context.beardifier();
        if (bound == nullptr) {
            output.fill(0.0F);
        } else {
            bound->sampleVolume(context, output, volume);
        }
    }
};

/// parity: `NoiseFunction.Sampler`.
class UnshiftedNoiseSampler final : public DensitySampler {
public:
    UnshiftedNoiseSampler(std::shared_ptr<const noise::Noise> noise, double xzScale, double yScale)
        : noise_(std::move(noise)), xzScale_(xzScale), yScale_(yScale) {}

    [[nodiscard]] float sampleValue(SamplerContext&, std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return noise_->get(static_cast<double>(x) * xzScale_, static_cast<double>(y) * yScale_,
                           static_cast<double>(z) * xzScale_);
    }

    void sampleVolume(SamplerContext&, DensityBuffer& output, const DensityVolume& volume) const override {
        output.fill(0.0F);
        noise_->addToVolume(output, volume, xzScale_, yScale_, 1.0F);
    }

private:
    std::shared_ptr<const noise::Noise> noise_;
    double xzScale_;
    double yScale_;
};

/// parity: `NoiseFunction.ShiftedXzSampler` and `ShiftedXyzSampler`. A null `shiftY` is the xz one.
class ShiftedNoiseSampler final : public DensitySampler {
public:
    ShiftedNoiseSampler(Sampler shiftX, Sampler shiftY, Sampler shiftZ, std::shared_ptr<const noise::Noise> noise,
                        double xzScale, double yScale)
        : shiftX_(std::move(shiftX)), shiftY_(std::move(shiftY)), shiftZ_(std::move(shiftZ)), noise_(std::move(noise)),
          xzScale_(xzScale), yScale_(yScale) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const double noiseX =
            static_cast<double>(x) * xzScale_ + static_cast<double>(shiftX_->sampleValue(context, x, y, z));
        double noiseY = static_cast<double>(y) * yScale_;
        if (shiftY_ != nullptr) {
            noiseY += static_cast<double>(shiftY_->sampleValue(context, x, y, z));
        }
        const double noiseZ =
            static_cast<double>(z) * xzScale_ + static_cast<double>(shiftZ_->sampleValue(context, x, y, z));
        return noise_->get(noiseX, noiseY, noiseZ);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        shiftX_->sampleVolume(context, output, volume);
        std::optional<ScopedBuffer> shiftY;
        if (shiftY_ != nullptr) {
            shiftY.emplace(context.acquire(volume.size()));
            shiftY_->sampleVolume(context, shiftY->buffer(), volume);
        }
        ScopedBuffer shiftZ = context.acquire(volume.size());
        shiftZ_->sampleVolume(context, shiftZ.buffer(), volume);
        std::int32_t index = 0;
        for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
            const double baseZ = static_cast<double>(volume.blockZ(z)) * xzScale_;
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                const double baseX = static_cast<double>(volume.blockX(x)) * xzScale_;
                for (std::int32_t y = 0; y < volume.sizeY; ++y) {
                    const double noiseX = baseX + static_cast<double>(output.get(index));
                    double noiseY = static_cast<double>(volume.blockY(y)) * yScale_;
                    if (shiftY.has_value()) {
                        noiseY += static_cast<double>(shiftY->get(index));
                    }
                    const double noiseZ = baseZ + static_cast<double>(shiftZ.get(index));
                    output.set(index, noise_->get(noiseX, noiseY, noiseZ));
                    ++index;
                }
            }
        }
    }

private:
    Sampler shiftX_;
    Sampler shiftY_;
    Sampler shiftZ_;
    std::shared_ptr<const noise::Noise> noise_;
    double xzScale_;
    double yScale_;
};

/// parity: `ShiftNoiseFunction.ShiftB`'s sampler, which reads the noise with x and z swapped.
class ShiftBSampler final : public DensitySampler {
public:
    explicit ShiftBSampler(std::shared_ptr<const noise::Noise> noise) : noise_(std::move(noise)) {}

    [[nodiscard]] float sampleValue(SamplerContext&, std::int32_t x, std::int32_t, std::int32_t z) const override {
        return noise_->get(static_cast<double>(z) * 0.25, static_cast<double>(x) * 0.25, 0.0) * 4.0F;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        const DensityVolume transposed{.sizeX = volume.sizeZ,
                                       .sizeY = volume.sizeX,
                                       .sizeZ = 1,
                                       .minBlockX = volume.minBlockZ,
                                       .minBlockY = volume.minBlockX,
                                       .minBlockZ = 0,
                                       .stepBlockX = volume.stepBlockZ,
                                       .stepBlockY = volume.stepBlockX,
                                       .stepBlockZ = 1};
        ScopedBuffer buffer = context.acquire(transposed.size());
        buffer.buffer().fill(0.0F);
        noise_->addToVolume(buffer.buffer(), transposed, 0.25, 0.25, 4.0F);
        for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                output.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY,
                                buffer.get(transposed.indexUnchecked(z, x, 0)));
            }
        }
    }

private:
    std::shared_ptr<const noise::Noise> noise_;
};

/// parity: `EndIslandFunction.Sampler`.
class EndIslandSampler final : public DensitySampler {
public:
    explicit EndIslandSampler(std::shared_ptr<const noise::SimplexNoise> noise) : noise_(std::move(noise)) {}

    [[nodiscard]] float sampleValue(SamplerContext&, std::int32_t x, std::int32_t, std::int32_t z) const override {
        return (heightValue(x / 8, z / 8) - 8.0F) / 128.0F;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
            const std::int32_t blockZ = volume.blockZ(z);
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                const float value = sampleValue(context, volume.blockX(x), 0, blockZ);
                output.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY, value);
            }
        }
    }

private:
    [[nodiscard]] float heightValue(std::int32_t sectionX, std::int32_t sectionZ) const {
        const std::int32_t chunkX = sectionX / 2;
        const std::int32_t chunkZ = sectionZ / 2;
        const std::int32_t subX = sectionX % 2;
        const std::int32_t subZ = sectionZ % 2;
        float offsets = -100.0F;
        for (std::int32_t xo = -12; xo <= 12; ++xo) {
            for (std::int32_t zo = -12; zo <= 12; ++zo) {
                const auto totalX = static_cast<std::int64_t>(chunkX + xo);
                const auto totalZ = static_cast<std::int64_t>(chunkZ + zo);
                if (totalX * totalX + totalZ * totalZ > 4096 &&
                    noise_->get(static_cast<double>(totalX), static_cast<double>(totalZ)) < -0.9F) {
                    const float islandSize = std::fmod(std::fabs(static_cast<float>(totalX)) * 3439.0F +
                                                           std::fabs(static_cast<float>(totalZ)) * 147.0F,
                                                       13.0F) +
                                             9.0F;
                    const auto xd = static_cast<float>(subX - xo * 2);
                    const auto zd = static_cast<float>(subZ - zo * 2);
                    const float distance = static_cast<float>(std::sqrt(static_cast<double>(xd * xd + zd * zd)));
                    float next = 100.0F - distance * islandSize;
                    next = jm::clamp(next, -100.0F, 80.0F);
                    offsets = jm::fmax(offsets, next);
                }
            }
        }
        return offsets;
    }

    std::shared_ptr<const noise::SimplexNoise> noise_;
};

class DistanceSampler final : public DensitySampler {
public:
    DistanceSampler(std::int32_t x, std::int32_t y, std::int32_t z, DistanceMetric metric)
        : x_(x), y_(y), z_(z), metric_(metric) {}

    [[nodiscard]] float sampleValue(SamplerContext&, std::int32_t x, std::int32_t y, std::int32_t z) const override {
        const auto dx = static_cast<float>(x_ - x);
        const auto dy = static_cast<float>(y_ - y);
        const auto dz = static_cast<float>(z_ - z);
        switch (metric_) {
        case DistanceMetric::Euclidean:
            return static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));
        case DistanceMetric::EuclideanSquared:
            return dx * dx + dy * dy + dz * dz;
        case DistanceMetric::Manhattan:
            return std::fabs(dx) + std::fabs(dy) + std::fabs(dz);
        case DistanceMetric::Chebyshev:
            return jm::fmax(jm::fmax(std::fabs(dx), std::fabs(dy)), std::fabs(dz));
        }
        return 0.0F;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        std::int32_t index = 0;
        for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                for (std::int32_t y = 0; y < volume.sizeY; ++y) {
                    output.set(index++, sampleValue(context, volume.blockX(x), volume.blockY(y), volume.blockZ(z)));
                }
            }
        }
    }

private:
    std::int32_t x_;
    std::int32_t y_;
    std::int32_t z_;
    DistanceMetric metric_;
};

/// parity: `GradientFunction`'s three samplers, one class with the tiling as a switch.
class GradientSampler final : public DensitySampler {
public:
    GradientSampler(Axis axis, Tiling tiling, std::int32_t from, std::int32_t to, float fromValue, float toValue)
        : axis_(axis), tiling_(tiling), from_(from), range_(to - from), min_(std::min(from, to)),
          max_(std::max(from, to)), fromValue_(fromValue),
          factor_((toValue - fromValue) / static_cast<float>(to - from)) {}

    [[nodiscard]] float compute(std::int32_t coordinate) const noexcept {
        switch (tiling_) {
        case Tiling::ClampToEdge:
            return fromValue_ + static_cast<float>(std::clamp(coordinate, min_, max_) - from_) * factor_;
        case Tiling::Repeat:
            return fromValue_ + static_cast<float>(jm::floorMod(coordinate - from_, range_)) * factor_;
        case Tiling::MirroredRepeat: {
            const std::int32_t relative = coordinate - from_;
            const std::int32_t tile = jm::floorDiv(relative, range_);
            const std::int32_t local = relative - tile * range_;
            return (tile & 1) == 0 ? fromValue_ + static_cast<float>(local) * factor_
                                   : fromValue_ + static_cast<float>(range_ - local) * factor_;
        }
        }
        return 0.0F;
    }

    [[nodiscard]] float sampleValue(SamplerContext&, std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return compute(axis_ == Axis::X ? x : (axis_ == Axis::Y ? y : z));
    }

    void sampleVolume(SamplerContext&, DensityBuffer& output, const DensityVolume& volume) const override {
        switch (axis_) {
        case Axis::X:
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                const float value = compute(volume.blockX(x));
                for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
                    output.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY, value);
                }
            }
            break;
        case Axis::Y:
            for (std::int32_t y = 0; y < volume.sizeY; ++y) {
                const float value = compute(volume.blockY(y));
                for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
                    for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                        output.set(volume.indexUnchecked(x, y, z), value);
                    }
                }
            }
            break;
        case Axis::Z:
            for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
                output.setRange(volume.indexUnchecked(0, 0, z), volume.sizeX * volume.sizeY, compute(volume.blockZ(z)));
            }
            break;
        }
    }

private:
    Axis axis_;
    Tiling tiling_;
    std::int32_t from_;
    std::int32_t range_;
    std::int32_t min_;
    std::int32_t max_;
    float fromValue_;
    float factor_;
};

/// parity: `BlendedNoise.createFbm`, octaves from the coarsest down with doubling amplitude.
[[nodiscard]] std::shared_ptr<const noise::NoiseStack> createFbm(random::RandomSource& random, std::int32_t firstOctave,
                                                                 double smearScaleY, double valueFactor) {
    const std::int32_t octaves = -firstOctave + 1;
    double factor = 1.0;
    valueFactor /= std::pow(2.0, static_cast<double>(octaves)) - 1.0;
    std::vector<noise::NoiseStack::Layer> layers;
    for (std::int32_t i = octaves - 1; i >= 0; --i) {
        layers.push_back({std::make_shared<const noise::SmearedPerlinNoise>(random, smearScaleY * factor), factor,
                          static_cast<float>(valueFactor)});
        factor /= 2.0;
        valueFactor *= 2.0;
    }
    return std::make_shared<const noise::NoiseStack>(std::move(layers));
}

constexpr double kBaseScale = 684.412;
const double kLimitFactor = static_cast<double>(0.99998474F);
constexpr double kMainFactor = 12.75;

} // namespace

std::size_t hashFloat(float value) noexcept { return std::hash<std::uint32_t>{}(std::bit_cast<std::uint32_t>(value)); }

std::size_t hashDouble(double value) noexcept {
    return std::hash<std::uint64_t>{}(std::bit_cast<std::uint64_t>(value));
}

bool sameFloat(float a, float b) noexcept { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }

bool sameDouble(double a, double b) noexcept {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

Sampler constantSampler(float value) { return std::make_shared<const ConstantSampler>(value); }

Sampler noiseSampler(std::shared_ptr<const noise::Noise> noise, double xzScale, double yScale) {
    return std::make_shared<const UnshiftedNoiseSampler>(std::move(noise), xzScale, yScale);
}

Sampler Constant::compile(CompileContext&) const { return constantSampler(value_); }

bool Constant::equals(const DensityFunction& other) const {
    return other.kind() == Kind::Constant && sameFloat(value_, *other.constantValue());
}

Interval Simple::range() const {
    switch (type_) {
    case SimpleType::BlendAlpha:
        return Interval::of(0.0F, 1.0F);
    case SimpleType::BlendOffset:
    case SimpleType::Beardifier:
        return Interval::infinite();
    }
    return Interval::infinite();
}

Sampler Simple::compile(CompileContext&) const {
    // parity: there is no blending, so both blend fields take their fallback value.
    switch (type_) {
    case SimpleType::BlendAlpha:
        return constantSampler(1.0F);
    case SimpleType::BlendOffset:
        return constantSampler(0.0F);
    case SimpleType::Beardifier:
        return std::make_shared<const BeardifierSampler>();
    }
    return constantSampler(0.0F);
}

bool Simple::equals(const DensityFunction& other) const {
    return other.kind() == Kind::Simple && static_cast<const Simple&>(other).type_ == type_;
}

std::int32_t NoiseNode::domainAxes() const {
    std::int32_t axes = kAllAxes;
    if (yScale_ == 0.0) {
        axes &= ~kAxisY;
    }
    if (xzScale_ == 0.0) {
        axes &= ~(kAxisX | kAxisZ);
    }
    return axes | shiftX_->domainAxes() | shiftY_->domainAxes() | shiftZ_->domainAxes();
}

Sampler NoiseNode::compile(CompileContext& context) const {
    std::shared_ptr<const noise::Noise> noise = context.createNoise(id_);
    const auto isZero = [](const Function& function) {
        const std::optional<float> value = function->constantValue();
        return value.has_value() && sameFloat(*value, 0.0F);
    };
    if (isZero(shiftX_) && isZero(shiftY_) && isZero(shiftZ_)) {
        return noiseSampler(std::move(noise), xzScale_, yScale_);
    }
    Sampler shiftX = shiftX_->compile(context);
    Sampler shiftZ = shiftZ_->compile(context);
    Sampler shiftY = isZero(shiftY_) ? nullptr : shiftY_->compile(context);
    return std::make_shared<const ShiftedNoiseSampler>(std::move(shiftX), std::move(shiftY), std::move(shiftZ),
                                                       std::move(noise), xzScale_, yScale_);
}

Function NoiseNode::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function x = rewriteChild(shiftX_, rule, changed);
    Function y = rewriteChild(shiftY_, rule, changed);
    Function z = rewriteChild(shiftZ_, rule, changed);
    return changed ? std::make_shared<const NoiseNode>(id_, parameters_, xzScale_, yScale_, x, y, z) : self;
}

bool NoiseNode::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Noise) {
        return false;
    }
    const auto& that = static_cast<const NoiseNode&>(other);
    return id_ == that.id_ && sameDouble(xzScale_, that.xzScale_) && sameDouble(yScale_, that.yScale_) &&
           equal(shiftX_, that.shiftX_) && equal(shiftY_, that.shiftY_) && equal(shiftZ_, that.shiftZ_);
}

std::size_t NoiseNode::hash() const {
    std::size_t seed = mix(4, std::hash<std::string>{}(id_));
    seed = mix(seed, hashDouble(xzScale_));
    seed = mix(seed, hashDouble(yScale_));
    return mix(mix(mix(seed, shiftX_->hash()), shiftY_->hash()), shiftZ_->hash());
}

Sampler EndIslands::compile(CompileContext& context) const {
    random::RandomSource random = context.createEndIslandRandom();
    random.consumeCount(17292);
    return std::make_shared<const EndIslandSampler>(std::make_shared<const noise::SimplexNoise>(random, true));
}

Interval DistanceToPoint::range() const { return Interval::of(0.0F, std::numeric_limits<float>::infinity()); }

Sampler DistanceToPoint::compile(CompileContext&) const {
    return std::make_shared<const DistanceSampler>(x_, y_, z_, metric_);
}

bool DistanceToPoint::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::DistanceToPoint) {
        return false;
    }
    const auto& that = static_cast<const DistanceToPoint&>(other);
    return x_ == that.x_ && y_ == that.y_ && z_ == that.z_ && metric_ == that.metric_;
}

std::size_t DistanceToPoint::hash() const {
    return mix(
        mix(mix(mix(6, static_cast<std::size_t>(x_)), static_cast<std::size_t>(y_)), static_cast<std::size_t>(z_)),
        static_cast<std::size_t>(metric_));
}

Sampler Gradient::compile(CompileContext&) const {
    return std::make_shared<const GradientSampler>(axis_, tiling_, from_, to_, fromValue_, toValue_);
}

bool Gradient::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Gradient) {
        return false;
    }
    const auto& that = static_cast<const Gradient&>(other);
    return axis_ == that.axis_ && tiling_ == that.tiling_ && from_ == that.from_ && to_ == that.to_ &&
           sameFloat(fromValue_, that.fromValue_) && sameFloat(toValue_, that.toValue_);
}

std::size_t Gradient::hash() const {
    std::size_t seed = mix(7, static_cast<std::size_t>(axis_));
    seed = mix(seed, static_cast<std::size_t>(tiling_));
    seed = mix(mix(seed, static_cast<std::size_t>(from_)), static_cast<std::size_t>(to_));
    return mix(mix(seed, hashFloat(fromValue_)), hashFloat(toValue_));
}

Sampler Shift::compile(CompileContext& context) const {
    std::shared_ptr<const noise::Noise> noise = context.createNoise(id_);
    switch (type_) {
    case ShiftType::ShiftA:
        return constMulSampler(noiseSampler(std::move(noise), 0.25, 0.0), 4.0F);
    case ShiftType::Shift:
        return constMulSampler(noiseSampler(std::move(noise), 0.25, 0.25), 4.0F);
    case ShiftType::ShiftB:
        return std::make_shared<const ShiftBSampler>(std::move(noise));
    }
    return constantSampler(0.0F);
}

bool Shift::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Shift) {
        return false;
    }
    const auto& that = static_cast<const Shift&>(other);
    return type_ == that.type_ && id_ == that.id_;
}

std::size_t Shift::hash() const { return mix(mix(8, static_cast<std::size_t>(type_)), std::hash<std::string>{}(id_)); }

Interval BlendedNoiseNode::range() const {
    const double smearScaleY = kBaseScale * yScale_ * smear_;
    const std::int32_t octaves = 16;
    double factor = 1.0;
    double valueFactor = kLimitFactor / (std::pow(2.0, static_cast<double>(octaves)) - 1.0);
    Interval range = Interval::ofExact(0.0F);
    for (std::int32_t i = octaves - 1; i >= 0; --i) {
        const Interval layer = Interval::mul(noise::SmearedPerlinNoise::rangeFor(smearScaleY * factor),
                                             Interval::ofExact(static_cast<float>(valueFactor)));
        range = Interval::add(range, layer);
        factor /= 2.0;
        valueFactor *= 2.0;
    }
    return range;
}

Sampler BlendedNoiseNode::compile(CompileContext& context) const {
    random::RandomSource random = context.createRandom("minecraft:terrain");
    return compileWith(random);
}

Sampler BlendedNoiseNode::compileWith(random::RandomSource& random) const {
    const double xzMultiplier = kBaseScale * xzScale_;
    const double yMultiplier = kBaseScale * yScale_;
    const double limitSmearScaleY = yMultiplier * smear_;
    const double mainSmearScaleY = limitSmearScaleY / yFactor_;
    auto minLimit = createFbm(random, -15, limitSmearScaleY, kLimitFactor);
    auto maxLimit = createFbm(random, -15, limitSmearScaleY, kLimitFactor);
    auto main = createFbm(random, -7, mainSmearScaleY, kMainFactor);
    Sampler minSampler = noiseSampler(std::move(minLimit), xzMultiplier, yMultiplier);
    Sampler maxSampler = noiseSampler(std::move(maxLimit), xzMultiplier, yMultiplier);
    Sampler mainSampler = noiseSampler(std::move(main), xzMultiplier / xzFactor_, yMultiplier / yFactor_);
    Sampler choice = clampSampler(constAddSampler(std::move(mainSampler), 0.5F), 0.0F, 1.0F);
    return lerpSampler(std::move(choice), std::move(minSampler), std::move(maxSampler));
}

bool BlendedNoiseNode::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::BlendedNoise) {
        return false;
    }
    const auto& that = static_cast<const BlendedNoiseNode&>(other);
    return sameDouble(xzScale_, that.xzScale_) && sameDouble(yScale_, that.yScale_) &&
           sameDouble(xzFactor_, that.xzFactor_) && sameDouble(yFactor_, that.yFactor_) &&
           sameDouble(smear_, that.smear_);
}

std::size_t BlendedNoiseNode::hash() const {
    return mix(
        mix(mix(mix(mix(9, hashDouble(xzScale_)), hashDouble(yScale_)), hashDouble(xzFactor_)), hashDouble(yFactor_)),
        hashDouble(smear_));
}

} // namespace basalt::server::world::densityfunction::nodes
