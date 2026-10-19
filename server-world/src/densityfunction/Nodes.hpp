#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::densityfunction::nodes {

using noise::Interval;

[[nodiscard]] inline std::size_t mix(std::size_t seed, std::size_t value) noexcept {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

[[nodiscard]] std::size_t hashFloat(float value) noexcept;
[[nodiscard]] std::size_t hashDouble(double value) noexcept;
[[nodiscard]] bool sameFloat(float a, float b) noexcept;
[[nodiscard]] bool sameDouble(double a, double b) noexcept;

[[nodiscard]] inline Function rewriteChild(const Function& child, const RewriteRule& rule, bool& changed) {
    Function rewritten = rule.rewrite(child);
    changed = changed || rewritten.get() != child.get();
    return rewritten;
}

class Constant final : public DensityFunction {
public:
    explicit Constant(float value) : value_(value) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Constant; }

    [[nodiscard]] Interval range() const override { return Interval::ofExact(value_); }

    [[nodiscard]] std::int32_t domainAxes() const override { return 0; }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;

    [[nodiscard]] std::size_t hash() const override { return mix(1, hashFloat(value_)); }

    [[nodiscard]] std::optional<float> constantValue() const override { return value_; }

private:
    float value_;
};

class Reference final : public DensityFunction {
public:
    Reference(std::string id, Function target) : id_(std::move(id)), target_(std::move(target)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Reference; }

    [[nodiscard]] Interval range() const override { return target_->range(); }

    [[nodiscard]] std::int32_t domainAxes() const override { return target_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override { return target_->compile(context); }

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;

    [[nodiscard]] std::size_t hash() const override { return mix(2, std::hash<std::string>{}(id_)); }

    [[nodiscard]] const Function& target() const noexcept { return target_; }

private:
    std::string id_;
    Function target_;
};

class Simple final : public DensityFunction {
public:
    explicit Simple(SimpleType type) : type_(type) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Simple; }

    [[nodiscard]] Interval range() const override;

    [[nodiscard]] std::int32_t domainAxes() const override {
        return type_ == SimpleType::Beardifier ? kAllAxes : kAxisX | kAxisZ;
    }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;

    [[nodiscard]] std::size_t hash() const override { return mix(3, static_cast<std::size_t>(type_)); }

private:
    SimpleType type_;
};

class NoiseNode final : public DensityFunction {
public:
    NoiseNode(std::string id, std::shared_ptr<const noise::NormalNoise> parameters, double xzScale, double yScale,
              Function shiftX, Function shiftY, Function shiftZ)
        : id_(std::move(id)), parameters_(std::move(parameters)), xzScale_(xzScale), yScale_(yScale),
          shiftX_(std::move(shiftX)), shiftY_(std::move(shiftY)), shiftZ_(std::move(shiftZ)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Noise; }

    [[nodiscard]] Interval range() const override { return parameters_->range(); }

    [[nodiscard]] std::int32_t domainAxes() const override;
    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    std::string id_;
    std::shared_ptr<const noise::NormalNoise> parameters_;
    double xzScale_;
    double yScale_;
    Function shiftX_;
    Function shiftY_;
    Function shiftZ_;
};

class EndIslands final : public DensityFunction {
public:
    [[nodiscard]] Kind kind() const noexcept override { return Kind::EndIslands; }

    [[nodiscard]] Interval range() const override { return Interval::of(-0.84375F, 0.5625F); }

    [[nodiscard]] std::int32_t domainAxes() const override { return kAxisX | kAxisZ; }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override { return other.kind() == Kind::EndIslands; }

    [[nodiscard]] std::size_t hash() const override { return 5; }
};

class DistanceToPoint final : public DensityFunction {
public:
    DistanceToPoint(std::int32_t x, std::int32_t y, std::int32_t z, DistanceMetric metric)
        : x_(x), y_(y), z_(z), metric_(metric) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::DistanceToPoint; }

    [[nodiscard]] Interval range() const override;

    [[nodiscard]] std::int32_t domainAxes() const override { return kAllAxes; }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    std::int32_t x_;
    std::int32_t y_;
    std::int32_t z_;
    DistanceMetric metric_;
};

class Gradient final : public DensityFunction {
public:
    Gradient(Axis axis, Tiling tiling, std::int32_t from, std::int32_t to, float fromValue, float toValue)
        : axis_(axis), tiling_(tiling), from_(from), to_(to), fromValue_(fromValue), toValue_(toValue) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Gradient; }

    [[nodiscard]] Interval range() const override { return Interval::encapsulating(fromValue_, toValue_); }

    [[nodiscard]] std::int32_t domainAxes() const override { return axesFrom(axis_); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Axis axis_;
    Tiling tiling_;
    std::int32_t from_;
    std::int32_t to_;
    float fromValue_;
    float toValue_;
};

class Shift final : public DensityFunction {
public:
    Shift(ShiftType type, std::string id, std::shared_ptr<const noise::NormalNoise> parameters)
        : type_(type), id_(std::move(id)), parameters_(std::move(parameters)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Shift; }

    [[nodiscard]] Interval range() const override {
        return Interval::mul(parameters_->range(), Interval::ofExact(4.0F));
    }

    [[nodiscard]] std::int32_t domainAxes() const override {
        return type_ == ShiftType::Shift ? kAllAxes : kAxisX | kAxisZ;
    }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    ShiftType type_;
    std::string id_;
    std::shared_ptr<const noise::NormalNoise> parameters_;
};

class BlendedNoiseNode final : public DensityFunction {
public:
    BlendedNoiseNode(double xzScale, double yScale, double xzFactor, double yFactor, double smear)
        : xzScale_(xzScale), yScale_(yScale), xzFactor_(xzFactor), yFactor_(yFactor), smear_(smear) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::BlendedNoise; }

    [[nodiscard]] Interval range() const override;

    [[nodiscard]] std::int32_t domainAxes() const override { return kAllAxes; }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

    /// parity: `BlendedNoise.compileSampler(RandomSource)`.
    [[nodiscard]] Sampler compileWith(random::RandomSource& random) const;

private:
    double xzScale_;
    double yScale_;
    double xzFactor_;
    double yFactor_;
    double smear_;
};

class Unary final : public DensityFunction {
public:
    Unary(UnaryType type, Function input) : type_(type), input_(std::move(input)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Unary; }

    [[nodiscard]] Interval range() const override;

    [[nodiscard]] std::int32_t domainAxes() const override { return input_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    UnaryType type_;
    Function input_;
};

class Binary final : public DensityFunction {
public:
    Binary(BinaryType type, Function left, Function right)
        : type_(type), left_(std::move(left)), right_(std::move(right)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Binary; }

    [[nodiscard]] Interval range() const override;

    [[nodiscard]] std::int32_t domainAxes() const override { return left_->domainAxes() | right_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    BinaryType type_;
    Function left_;
    Function right_;
};

class SplineNode final : public DensityFunction {
public:
    explicit SplineNode(std::shared_ptr<const Spline> spline) : spline_(std::move(spline)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Spline; }

    [[nodiscard]] Interval range() const override;
    [[nodiscard]] std::int32_t domainAxes() const override;
    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    std::shared_ptr<const Spline> spline_;
};

class Lerp final : public DensityFunction {
public:
    Lerp(Function alpha, Function first, Function second)
        : alpha_(std::move(alpha)), first_(std::move(first)), second_(std::move(second)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Lerp; }

    [[nodiscard]] Interval range() const override {
        return Interval::lerp(alpha_->range(), first_->range(), second_->range());
    }

    [[nodiscard]] std::int32_t domainAxes() const override {
        return alpha_->domainAxes() | first_->domainAxes() | second_->domainAxes();
    }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Function alpha_;
    Function first_;
    Function second_;
};

class Clamp final : public DensityFunction {
public:
    Clamp(Function input, float min, float max) : input_(std::move(input)), min_(min), max_(max) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Clamp; }

    [[nodiscard]] Interval range() const override { return Interval::clamp(input_->range(), min_, max_); }

    [[nodiscard]] std::int32_t domainAxes() const override { return input_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Function input_;
    float min_;
    float max_;
};

class RangeChoice final : public DensityFunction {
public:
    RangeChoice(Function input, float min, float max, Function inRange, Function outOfRange)
        : input_(std::move(input)), min_(min), max_(max), inRange_(std::move(inRange)),
          outOfRange_(std::move(outOfRange)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::RangeChoice; }

    [[nodiscard]] Interval range() const override {
        return Interval::encapsulating({inRange_->range(), outOfRange_->range()});
    }

    [[nodiscard]] std::int32_t domainAxes() const override {
        return input_->domainAxes() | inRange_->domainAxes() | outOfRange_->domainAxes();
    }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Function input_;
    float min_;
    float max_;
    Function inRange_;
    Function outOfRange_;
};

class IntervalSelect final : public DensityFunction {
public:
    IntervalSelect(Function input, std::vector<float> thresholds, std::vector<Function> functions)
        : input_(std::move(input)), thresholds_(std::move(thresholds)), functions_(std::move(functions)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::IntervalSelect; }

    [[nodiscard]] Interval range() const override;
    [[nodiscard]] std::int32_t domainAxes() const override;
    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Function input_;
    std::vector<float> thresholds_;
    std::vector<Function> functions_;
};

class Cache final : public DensityFunction {
public:
    explicit Cache(Function input) : input_(std::move(input)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Cache; }

    [[nodiscard]] Interval range() const override { return input_->range(); }

    [[nodiscard]] std::int32_t domainAxes() const override { return input_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;

    [[nodiscard]] std::size_t hash() const override { return mix(16, input_->hash()); }

    [[nodiscard]] const Function& input() const noexcept { return input_; }

private:
    Function input_;
};

/// parity: `DensityFunctionCompiler.PreparedCache`, a deduplicated cache with its id.
class PreparedCache final : public DensityFunction {
public:
    PreparedCache(std::int32_t id, Interval range, std::int32_t axes, Sampler sampler)
        : id_(id), range_(range), axes_(axes), sampler_(std::move(sampler)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::PreparedCache; }

    [[nodiscard]] Interval range() const override { return range_; }

    [[nodiscard]] std::int32_t domainAxes() const override { return axes_; }

    [[nodiscard]] Sampler compile(CompileContext&) const override { return sampler_; }

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule&) const override { return self; }

    [[nodiscard]] bool equals(const DensityFunction& other) const override;

    [[nodiscard]] std::size_t hash() const override { return mix(17, static_cast<std::size_t>(id_)); }

private:
    std::int32_t id_;
    Interval range_;
    std::int32_t axes_;
    Sampler sampler_;
};

class BlendDensity final : public DensityFunction {
public:
    explicit BlendDensity(Function input) : input_(std::move(input)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::BlendDensity; }

    [[nodiscard]] Interval range() const override { return input_->range(); }

    [[nodiscard]] std::int32_t domainAxes() const override { return input_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;

    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;

    [[nodiscard]] std::size_t hash() const override { return mix(18, input_->hash()); }

private:
    Function input_;
};

class Interpolated final : public DensityFunction {
public:
    Interpolated(Function input, std::int32_t cellXz, std::int32_t cellY)
        : input_(std::move(input)), cellXz_(cellXz), cellY_(cellY) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Interpolated; }

    [[nodiscard]] Interval range() const override { return input_->range(); }

    [[nodiscard]] std::int32_t domainAxes() const override { return input_->domainAxes(); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Function input_;
    std::int32_t cellXz_;
    std::int32_t cellY_;
};

class Slice final : public DensityFunction {
public:
    Slice(Axis axis, std::int32_t coordinate, Function input)
        : axis_(axis), coordinate_(coordinate), input_(std::move(input)) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::Slice; }

    [[nodiscard]] Interval range() const override { return input_->range(); }

    [[nodiscard]] std::int32_t domainAxes() const override { return input_->domainAxes() & ~axesFrom(axis_); }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

    [[nodiscard]] Axis axis() const noexcept { return axis_; }

    [[nodiscard]] const Function& input() const noexcept { return input_; }

private:
    Axis axis_;
    std::int32_t coordinate_;
    Function input_;
};

class FindTopSurface final : public DensityFunction {
public:
    FindTopSurface(Function density, Function upperBound, std::int32_t lowerBound, std::int32_t cellHeight)
        : density_(std::move(density)), upperBound_(std::move(upperBound)), lowerBound_(lowerBound),
          cellHeight_(cellHeight) {}

    [[nodiscard]] Kind kind() const noexcept override { return Kind::FindTopSurface; }

    [[nodiscard]] Interval range() const override;

    [[nodiscard]] std::int32_t domainAxes() const override {
        return (density_->domainAxes() | upperBound_->domainAxes()) & ~kAxisY;
    }

    [[nodiscard]] Sampler compile(CompileContext& context) const override;
    [[nodiscard]] Function rewriteChildren(const Function& self, const RewriteRule& rule) const override;
    [[nodiscard]] bool equals(const DensityFunction& other) const override;
    [[nodiscard]] std::size_t hash() const override;

private:
    Function density_;
    Function upperBound_;
    std::int32_t lowerBound_;
    std::int32_t cellHeight_;
};

[[nodiscard]] Sampler constantSampler(float value);
[[nodiscard]] Sampler noiseSampler(std::shared_ptr<const noise::Noise> noise, double xzScale, double yScale);
[[nodiscard]] Sampler constAddSampler(Sampler input, float value);
[[nodiscard]] Sampler constMulSampler(Sampler input, float value);
[[nodiscard]] Sampler clampSampler(Sampler input, float min, float max);
[[nodiscard]] Sampler lerpSampler(Sampler alpha, Sampler first, Sampler second);
[[nodiscard]] Sampler ySliceSampler(Sampler input, std::int32_t y);
[[nodiscard]] Sampler makeCachingSampler(std::int32_t id, Sampler input);

} // namespace basalt::server::world::densityfunction::nodes
