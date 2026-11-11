#include "Nodes.hpp"

#include <basalt/server/world/noise/JavaMath.hpp>

#include <cmath>

namespace basalt::server::world::densityfunction::nodes {
namespace {

namespace jm = noise::javaMath;

[[nodiscard]] float powFloat(double base, double exponent) noexcept {
    return static_cast<float>(jm::pow(base, exponent));
}

[[nodiscard]] float powFloat(float base, float exponent) noexcept {
    return powFloat(static_cast<double>(base), static_cast<double>(exponent));
}

[[nodiscard]] float powFloat(double base, float exponent) noexcept {
    return powFloat(base, static_cast<double>(exponent));
}

[[nodiscard]] float powFloat(float base, double exponent) noexcept {
    return powFloat(static_cast<double>(base), exponent);
}

class PowSampler final : public DensitySampler {
public:
    PowSampler(Sampler base, Sampler exponent) : base_(std::move(base)), exponent_(std::move(exponent)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float base = base_->sampleValue(context, x, y, z);
        return powFloat(base, exponent_->sampleValue(context, x, y, z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        base_->sampleVolume(context, output, volume);
        ScopedBuffer exponent = context.acquire(volume.size());
        exponent_->sampleVolume(context, exponent.buffer(), volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, powFloat(output.get(i), exponent.get(i)));
        }
    }

private:
    Sampler base_;
    Sampler exponent_;
};

class ConstBasePowSampler final : public DensitySampler {
public:
    ConstBasePowSampler(double base, Sampler exponent) : base_(base), exponent_(std::move(exponent)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return powFloat(base_, exponent_->sampleValue(context, x, y, z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        exponent_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, powFloat(base_, output.get(i)));
        }
    }

private:
    double base_;
    Sampler exponent_;
};

class ConstExponentPowSampler final : public DensitySampler {
public:
    ConstExponentPowSampler(Sampler base, double exponent) : base_(std::move(base)), exponent_(exponent) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return powFloat(base_->sampleValue(context, x, y, z), exponent_);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        base_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, powFloat(output.get(i), exponent_));
        }
    }

private:
    Sampler base_;
    double exponent_;
};

[[nodiscard]] Sampler constExponentSampler(Sampler base, float exponent) {
    const float magnitude = std::fabs(exponent);
    Sampler special;
    if (magnitude == 0.5F) {
        special = unarySampler(UnaryType::Sqrt, base);
    } else if (magnitude == 1.0F) {
        special = base;
    } else if (magnitude == 2.0F) {
        special = unarySampler(UnaryType::Square, base);
    } else if (magnitude == 3.0F) {
        special = unarySampler(UnaryType::Cube, base);
    } else {
        return std::make_shared<const ConstExponentPowSampler>(std::move(base), static_cast<double>(exponent));
    }
    return exponent >= 0.0F ? special : unarySampler(UnaryType::Reciprocal, special);
}

class RoundSampler final : public DensitySampler {
public:
    RoundSampler(RoundType type, Sampler input, Sampler multiple)
        : type_(type), input_(std::move(input)), multiple_(std::move(multiple)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float input = input_->sampleValue(context, x, y, z);
        return apply(input, multiple_->sampleValue(context, x, y, z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        ScopedBuffer multiple = context.acquire(volume.size());
        multiple_->sampleVolume(context, multiple.buffer(), volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, apply(output.get(i), multiple.get(i)));
        }
    }

private:
    [[nodiscard]] float apply(float input, float multiple) const noexcept {
        return multiple == 0.0F ? input : roundToInteger(input / multiple, type_) * multiple;
    }

    RoundType type_;
    Sampler input_;
    Sampler multiple_;
};

class IntegerRoundSampler final : public DensitySampler {
public:
    IntegerRoundSampler(RoundType type, Sampler input) : type_(type), input_(std::move(input)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return roundToInteger(input_->sampleValue(context, x, y, z), type_);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, roundToInteger(output.get(i), type_));
        }
    }

private:
    RoundType type_;
    Sampler input_;
};

} // namespace

float roundToInteger(float input, RoundType type) noexcept {
    const double value = static_cast<double>(input);
    switch (type) {
    case RoundType::Floor:
        return static_cast<float>(std::floor(value));
    case RoundType::Round:
        // parity: `Math.round(float)` saturates to int and maps NaN to zero.
        return static_cast<float>(jm::toInt(std::floor(value + 0.5)));
    case RoundType::Ceil:
        return static_cast<float>(std::ceil(value));
    case RoundType::Truncate:
        return input > 0.0F ? static_cast<float>(std::floor(value)) : static_cast<float>(std::ceil(value));
    }
    return input;
}

Sampler Pow::compile(CompileContext& context) const {
    Sampler base = base_->compile(context);
    Sampler exponent = exponent_->compile(context);
    if (const std::optional<float> constantBase = base_->constantValue(); constantBase.has_value()) {
        return std::make_shared<const ConstBasePowSampler>(static_cast<double>(*constantBase), std::move(exponent));
    }
    if (const std::optional<float> constantExponent = exponent_->constantValue(); constantExponent.has_value()) {
        return constExponentSampler(std::move(base), *constantExponent);
    }
    return std::make_shared<const PowSampler>(std::move(base), std::move(exponent));
}

Function Pow::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function base = rewriteChild(base_, rule, changed);
    Function exponent = rewriteChild(exponent_, rule, changed);
    return changed ? std::make_shared<const Pow>(base, exponent) : self;
}

bool Pow::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Pow) {
        return false;
    }
    const auto& that = static_cast<const Pow&>(other);
    return equal(base_, that.base_) && equal(exponent_, that.exponent_);
}

std::size_t Pow::hash() const { return mix(mix(16, base_->hash()), exponent_->hash()); }

Interval Round::range() const {
    const Interval multiple = multiple_->range();
    const RoundType type = type_;
    return Interval::mul(Interval::mapMonotonic(Interval::div(input_->range(), multiple),
                                                [type](float value) { return roundToInteger(value, type); }),
                         multiple);
}

Sampler Round::compile(CompileContext& context) const {
    Sampler input = input_->compile(context);
    if (const std::optional<float> multiple = multiple_->constantValue(); multiple.has_value() && *multiple == 1.0F) {
        return std::make_shared<const IntegerRoundSampler>(type_, std::move(input));
    }
    return std::make_shared<const RoundSampler>(type_, std::move(input), multiple_->compile(context));
}

Function Round::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    Function multiple = rewriteChild(multiple_, rule, changed);
    return changed ? std::make_shared<const Round>(type_, input, multiple) : self;
}

bool Round::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Round) {
        return false;
    }
    const auto& that = static_cast<const Round&>(other);
    return type_ == that.type_ && equal(input_, that.input_) && equal(multiple_, that.multiple_);
}

std::size_t Round::hash() const {
    return mix(mix(mix(17, static_cast<std::size_t>(type_)), input_->hash()), multiple_->hash());
}

} // namespace basalt::server::world::densityfunction::nodes
