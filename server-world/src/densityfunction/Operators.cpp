#include "Nodes.hpp"

#include <basalt/server/world/noise/JavaMath.hpp>

#include <cmath>

namespace basalt::server::world::densityfunction::nodes {
namespace {

namespace jm = noise::javaMath;

template <typename Op>
class MapSampler final : public DensitySampler {
public:
    MapSampler(Sampler input, Op op) : input_(std::move(input)), op_(op) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return op_(input_->sampleValue(context, x, y, z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, op_(output.get(i)));
        }
    }

private:
    Sampler input_;
    Op op_;
};

template <typename Op>
[[nodiscard]] Sampler mapped(Sampler input, Op op) {
    return std::make_shared<const MapSampler<Op>>(std::move(input), op);
}

[[nodiscard]] float squeeze(float input) noexcept {
    const float clamped = jm::clamp(input, -1.0F, 1.0F);
    return clamped / 2.0F - clamped * clamped * clamped / 24.0F;
}

[[nodiscard]] float leaky(float factor, float input) noexcept { return input > 0.0F ? input : input * factor; }

class BinarySampler final : public DensitySampler {
public:
    BinarySampler(BinaryType type, Sampler left, Sampler right, float rightBound)
        : type_(type), left_(std::move(left)), right_(std::move(right)), rightBound_(rightBound) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float left = left_->sampleValue(context, x, y, z);
        switch (type_) {
        case BinaryType::Add:
            return left + right_->sampleValue(context, x, y, z);
        case BinaryType::Sub:
            return left - right_->sampleValue(context, x, y, z);
        case BinaryType::Mul:
            return left == 0.0F ? 0.0F : left * right_->sampleValue(context, x, y, z);
        case BinaryType::Div:
            return left == 0.0F ? 0.0F : left / right_->sampleValue(context, x, y, z);
        case BinaryType::Min:
            return left <= rightBound_ ? left : jm::fmin(left, right_->sampleValue(context, x, y, z));
        case BinaryType::Max:
            return left >= rightBound_ ? left : jm::fmax(left, right_->sampleValue(context, x, y, z));
        }
        return left;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        left_->sampleVolume(context, output, volume);
        ScopedBuffer rightScope = context.acquire(volume.size());
        right_->sampleVolume(context, rightScope.buffer(), volume);
        const DensityBuffer& right = rightScope.buffer();
        for (std::int32_t i = 0; i < output.size(); ++i) {
            const float r = right.get(i);
            switch (type_) {
            case BinaryType::Add:
                output.addTo(i, r);
                break;
            case BinaryType::Sub:
                output.addTo(i, -r);
                break;
            case BinaryType::Mul:
                output.set(i, output.get(i) * r);
                break;
            case BinaryType::Div:
                output.set(i, output.get(i) / r);
                break;
            case BinaryType::Min:
                if (r < output.get(i)) {
                    output.set(i, r);
                }
                break;
            case BinaryType::Max:
                if (r > output.get(i)) {
                    output.set(i, r);
                }
                break;
            }
        }
    }

private:
    BinaryType type_;
    Sampler left_;
    Sampler right_;
    float rightBound_;
};

/// parity: `ConstMinSampler` and `ConstMaxSampler`: the point path is a plain `Math.min`.
class ConstMinMaxSampler final : public DensitySampler {
public:
    ConstMinMaxSampler(bool max, Sampler input, float value) : max_(max), input_(std::move(input)), value_(value) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float input = input_->sampleValue(context, x, y, z);
        return max_ ? jm::fmax(input, value_) : jm::fmin(input, value_);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            if (max_ ? value_ > output.get(i) : value_ < output.get(i)) {
                output.set(i, value_);
            }
        }
    }

private:
    bool max_;
    Sampler input_;
    float value_;
};

/// parity: `ConstSubSampler` and `ConstDivSampler`, a constant on the left.
class ConstLeftSampler final : public DensitySampler {
public:
    ConstLeftSampler(bool divide, float value, Sampler input)
        : divide_(divide), value_(value), input_(std::move(input)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float input = input_->sampleValue(context, x, y, z);
        return divide_ ? value_ / input : value_ - input;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, divide_ ? value_ / output.get(i) : value_ - output.get(i));
        }
    }

private:
    bool divide_;
    float value_;
    Sampler input_;
};

class ConstAddSampler final : public DensitySampler {
public:
    ConstAddSampler(Sampler input, float value) : input_(std::move(input)), value_(value) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return input_->sampleValue(context, x, y, z) + value_;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.addTo(i, value_);
        }
    }

private:
    Sampler input_;
    float value_;
};

class ConstMulSampler final : public DensitySampler {
public:
    ConstMulSampler(Sampler input, float value) : input_(std::move(input)), value_(value) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return input_->sampleValue(context, x, y, z) * value_;
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, output.get(i) * value_);
        }
    }

private:
    Sampler input_;
    float value_;
};

/// parity: `LerpFunction`'s samplers.
class LerpSampler final : public DensitySampler {
public:
    LerpSampler(Sampler alpha, Sampler first, Sampler second)
        : alpha_(std::move(alpha)), first_(std::move(first)), second_(std::move(second)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float alpha = alpha_->sampleValue(context, x, y, z);
        if (alpha == 0.0F) {
            return first_->sampleValue(context, x, y, z);
        }
        if (alpha == 1.0F) {
            return second_->sampleValue(context, x, y, z);
        }
        const float first = first_->sampleValue(context, x, y, z);
        return jm::lerp(alpha, first, second_->sampleValue(context, x, y, z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        alpha_->sampleVolume(context, output, volume);
        ScopedBuffer first = context.acquire(volume.size());
        first_->sampleVolume(context, first.buffer(), volume);
        ScopedBuffer second = context.acquire(volume.size());
        second_->sampleVolume(context, second.buffer(), volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            const float alpha = output.get(i);
            if (alpha == 0.0F) {
                output.set(i, first.get(i));
            } else if (alpha == 1.0F) {
                output.set(i, second.get(i));
            } else {
                output.set(i, jm::lerp(alpha, first.get(i), second.get(i)));
            }
        }
    }

private:
    Sampler alpha_;
    Sampler first_;
    Sampler second_;
};

class RangeChoiceSampler final : public DensitySampler {
public:
    RangeChoiceSampler(Sampler input, float min, float max, Sampler inRange, Sampler outOfRange)
        : input_(std::move(input)), min_(min), max_(max), inRange_(std::move(inRange)),
          outOfRange_(std::move(outOfRange)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const float input = input_->sampleValue(context, x, y, z);
        return input >= min_ && input < max_ ? inRange_->sampleValue(context, x, y, z)
                                             : outOfRange_->sampleValue(context, x, y, z);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        inRange_->sampleVolume(context, output, volume);
        ScopedBuffer input = context.acquire(volume.size());
        input_->sampleVolume(context, input.buffer(), volume);
        ScopedBuffer outOfRange = context.acquire(volume.size());
        outOfRange_->sampleVolume(context, outOfRange.buffer(), volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            const float value = input.get(i);
            if (!(value >= min_) || !(value < max_)) {
                output.set(i, outOfRange.get(i));
            }
        }
    }

private:
    Sampler input_;
    float min_;
    float max_;
    Sampler inRange_;
    Sampler outOfRange_;
};

/// parity: `RangeChoiceFunction.ConstSampler`.
class ConstRangeChoiceSampler final : public DensitySampler {
public:
    ConstRangeChoiceSampler(Sampler input, float min, float max, float inRange, float outOfRange)
        : input_(std::move(input)), min_(min), max_(max), inRange_(inRange), outOfRange_(outOfRange) {}

    [[nodiscard]] float choose(float input) const noexcept {
        return input >= min_ && input < max_ ? inRange_ : outOfRange_;
    }

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return choose(input_->sampleValue(context, x, y, z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, choose(output.get(i)));
        }
    }

private:
    Sampler input_;
    float min_;
    float max_;
    float inRange_;
    float outOfRange_;
};

class IntervalSelectSampler final : public DensitySampler {
public:
    IntervalSelectSampler(Sampler input, std::vector<float> thresholds, std::vector<Sampler> samplers)
        : input_(std::move(input)), thresholds_(std::move(thresholds)), samplers_(std::move(samplers)) {}

    [[nodiscard]] std::size_t select(float input) const noexcept {
        for (std::size_t i = 0; i < thresholds_.size(); ++i) {
            if (input < thresholds_[i]) {
                return i;
            }
        }
        return samplers_.size() - 1;
    }

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return samplers_[select(input_->sampleValue(context, x, y, z))]->sampleValue(context, x, y, z);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        input_->sampleVolume(context, output, volume);
        std::vector<ScopedBuffer> buffers;
        buffers.reserve(samplers_.size());
        for (const Sampler& sampler : samplers_) {
            buffers.push_back(context.acquire(volume.size()));
            sampler->sampleVolume(context, buffers.back().buffer(), volume);
        }
        for (std::int32_t i = 0; i < output.size(); ++i) {
            output.set(i, buffers[select(output.get(i))].get(i));
        }
    }

private:
    Sampler input_;
    std::vector<float> thresholds_;
    std::vector<Sampler> samplers_;
};

} // namespace

Sampler constAddSampler(Sampler input, float value) {
    return std::make_shared<const ConstAddSampler>(std::move(input), value);
}

Sampler constMulSampler(Sampler input, float value) {
    return std::make_shared<const ConstMulSampler>(std::move(input), value);
}

Sampler clampSampler(Sampler input, float min, float max) {
    return mapped(std::move(input), [min, max](float value) { return jm::clamp(value, min, max); });
}

Sampler lerpSampler(Sampler alpha, Sampler first, Sampler second) {
    return std::make_shared<const LerpSampler>(std::move(alpha), std::move(first), std::move(second));
}

Interval Unary::range() const {
    const Interval input = input_->range();
    switch (type_) {
    case UnaryType::Abs:
        return Interval::abs(input);
    case UnaryType::Square:
        return Interval::square(input);
    case UnaryType::Cube:
        return Interval::mapMonotonic(input, [](float x) { return x * x * x; });
    case UnaryType::Sqrt:
        return Interval::pow(input, Interval::ofExact(0.5F));
    case UnaryType::HalfNegative:
        return Interval::mapMonotonic(input, [](float x) { return leaky(0.5F, x); });
    case UnaryType::QuarterNegative:
        return Interval::mapMonotonic(input, [](float x) { return leaky(0.25F, x); });
    case UnaryType::Reciprocal:
        return Interval::reciprocal(input);
    case UnaryType::Negate:
        return Interval::sub(Interval::ofExact(0.0F), input);
    case UnaryType::Squeeze:
        return Interval::mapMonotonic(input, squeeze);
    case UnaryType::Log:
        return Interval::log(input);
    case UnaryType::Sign:
        return Interval::sign(input);
    }
    return input;
}

Sampler Unary::compile(CompileContext& context) const {
    Sampler input = input_->compile(context);
    switch (type_) {
    case UnaryType::Abs:
        return mapped(std::move(input), [](float x) { return std::fabs(x); });
    case UnaryType::Square:
        return mapped(std::move(input), [](float x) { return x * x; });
    case UnaryType::Cube:
        return mapped(std::move(input), [](float x) { return x * x * x; });
    case UnaryType::Sqrt:
        return mapped(std::move(input), [](float x) { return static_cast<float>(std::sqrt(static_cast<double>(x))); });
    case UnaryType::HalfNegative:
        return mapped(std::move(input), [](float x) { return leaky(0.5F, x); });
    case UnaryType::QuarterNegative:
        return mapped(std::move(input), [](float x) { return leaky(0.25F, x); });
    case UnaryType::Reciprocal:
        return mapped(std::move(input), [](float x) { return 1.0F / x; });
    case UnaryType::Negate:
        return mapped(std::move(input), [](float x) { return -x; });
    case UnaryType::Squeeze:
        return mapped(std::move(input), squeeze);
    case UnaryType::Log:
        return mapped(std::move(input), [](float x) { return static_cast<float>(std::log(static_cast<double>(x))); });
    case UnaryType::Sign:
        return mapped(std::move(input), [](float x) { return jm::signum(x); });
    }
    return input;
}

Function Unary::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    return changed ? std::make_shared<const Unary>(type_, input) : self;
}

bool Unary::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Unary) {
        return false;
    }
    const auto& that = static_cast<const Unary&>(other);
    return type_ == that.type_ && equal(input_, that.input_);
}

std::size_t Unary::hash() const { return mix(mix(10, static_cast<std::size_t>(type_)), input_->hash()); }

Interval Binary::range() const {
    const Interval left = left_->range();
    const Interval right = right_->range();
    switch (type_) {
    case BinaryType::Add:
        return Interval::add(left, right);
    case BinaryType::Sub:
        return Interval::sub(left, right);
    case BinaryType::Mul:
        return Interval::mul(left, right);
    case BinaryType::Div:
        return Interval::div(left, right);
    case BinaryType::Min:
        return Interval::min(left, right);
    case BinaryType::Max:
        return Interval::max(left, right);
    }
    return left;
}

Sampler Binary::compile(CompileContext& context) const {
    Sampler left = left_->compile(context);
    Sampler right = right_->compile(context);
    const std::optional<float> leftConstant = left_->constantValue();
    const std::optional<float> rightConstant = right_->constantValue();
    switch (type_) {
    case BinaryType::Add:
        if (leftConstant.has_value()) {
            return constAddSampler(std::move(right), *leftConstant);
        }
        if (rightConstant.has_value()) {
            return constAddSampler(std::move(left), *rightConstant);
        }
        break;
    case BinaryType::Sub:
        if (leftConstant.has_value()) {
            return std::make_shared<const ConstLeftSampler>(false, *leftConstant, std::move(right));
        }
        if (rightConstant.has_value()) {
            return constAddSampler(std::move(left), -*rightConstant);
        }
        break;
    case BinaryType::Mul:
        if (leftConstant.has_value()) {
            return constMulSampler(std::move(right), *leftConstant);
        }
        if (rightConstant.has_value()) {
            return constMulSampler(std::move(left), *rightConstant);
        }
        break;
    case BinaryType::Div:
        if (leftConstant.has_value()) {
            return std::make_shared<const ConstLeftSampler>(true, *leftConstant, std::move(right));
        }
        if (rightConstant.has_value()) {
            return constMulSampler(std::move(left), 1.0F / *rightConstant);
        }
        break;
    case BinaryType::Min:
    case BinaryType::Max: {
        const bool max = type_ == BinaryType::Max;
        const Interval leftRange = left_->range();
        const Interval rightRange = right_->range();
        // parity: disjoint ranges decide the answer at compile time.
        if (max ? leftRange.min() > rightRange.max() : leftRange.max() < rightRange.min()) {
            return left;
        }
        if (max ? rightRange.min() > leftRange.max() : rightRange.max() < leftRange.min()) {
            return right;
        }
        if (leftConstant.has_value()) {
            return std::make_shared<const ConstMinMaxSampler>(max, std::move(right), *leftConstant);
        }
        if (rightConstant.has_value()) {
            return std::make_shared<const ConstMinMaxSampler>(max, std::move(left), *rightConstant);
        }
        return std::make_shared<const BinarySampler>(type_, std::move(left), std::move(right),
                                                     max ? rightRange.max() : rightRange.min());
    }
    }
    return std::make_shared<const BinarySampler>(type_, std::move(left), std::move(right), 0.0F);
}

Function Binary::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function left = rewriteChild(left_, rule, changed);
    Function right = rewriteChild(right_, rule, changed);
    return changed ? std::make_shared<const Binary>(type_, left, right) : self;
}

bool Binary::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Binary) {
        return false;
    }
    const auto& that = static_cast<const Binary&>(other);
    return type_ == that.type_ && equal(left_, that.left_) && equal(right_, that.right_);
}

std::size_t Binary::hash() const {
    return mix(mix(mix(11, static_cast<std::size_t>(type_)), left_->hash()), right_->hash());
}

Sampler Lerp::compile(CompileContext& context) const {
    return lerpSampler(alpha_->compile(context), first_->compile(context), second_->compile(context));
}

Function Lerp::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function alpha = rewriteChild(alpha_, rule, changed);
    Function first = rewriteChild(first_, rule, changed);
    Function second = rewriteChild(second_, rule, changed);
    return changed ? std::make_shared<const Lerp>(alpha, first, second) : self;
}

bool Lerp::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Lerp) {
        return false;
    }
    const auto& that = static_cast<const Lerp&>(other);
    return equal(alpha_, that.alpha_) && equal(first_, that.first_) && equal(second_, that.second_);
}

std::size_t Lerp::hash() const { return mix(mix(mix(12, alpha_->hash()), first_->hash()), second_->hash()); }

Sampler Clamp::compile(CompileContext& context) const { return clampSampler(input_->compile(context), min_, max_); }

Function Clamp::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    return changed ? std::make_shared<const Clamp>(input, min_, max_) : self;
}

bool Clamp::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Clamp) {
        return false;
    }
    const auto& that = static_cast<const Clamp&>(other);
    return sameFloat(min_, that.min_) && sameFloat(max_, that.max_) && equal(input_, that.input_);
}

std::size_t Clamp::hash() const { return mix(mix(mix(13, input_->hash()), hashFloat(min_)), hashFloat(max_)); }

Sampler RangeChoice::compile(CompileContext& context) const {
    Sampler input = input_->compile(context);
    const std::optional<float> inRange = inRange_->constantValue();
    const std::optional<float> outOfRange = outOfRange_->constantValue();
    if (inRange.has_value() && outOfRange.has_value()) {
        return std::make_shared<const ConstRangeChoiceSampler>(std::move(input), min_, max_, *inRange, *outOfRange);
    }
    return std::make_shared<const RangeChoiceSampler>(std::move(input), min_, max_, inRange_->compile(context),
                                                      outOfRange_->compile(context));
}

Function RangeChoice::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    Function inRange = rewriteChild(inRange_, rule, changed);
    Function outOfRange = rewriteChild(outOfRange_, rule, changed);
    return changed ? std::make_shared<const RangeChoice>(input, min_, max_, inRange, outOfRange) : self;
}

bool RangeChoice::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::RangeChoice) {
        return false;
    }
    const auto& that = static_cast<const RangeChoice&>(other);
    return sameFloat(min_, that.min_) && sameFloat(max_, that.max_) && equal(input_, that.input_) &&
           equal(inRange_, that.inRange_) && equal(outOfRange_, that.outOfRange_);
}

std::size_t RangeChoice::hash() const {
    std::size_t seed = mix(mix(14, input_->hash()), hashFloat(min_));
    return mix(mix(mix(seed, hashFloat(max_)), inRange_->hash()), outOfRange_->hash());
}

Interval IntervalSelect::range() const {
    Interval range = Interval::notAnInterval();
    for (const Function& function : functions_) {
        range = Interval::encapsulating({range, function->range()});
    }
    return range;
}

std::int32_t IntervalSelect::domainAxes() const {
    std::int32_t axes = input_->domainAxes();
    for (const Function& function : functions_) {
        axes |= function->domainAxes();
    }
    return axes;
}

Sampler IntervalSelect::compile(CompileContext& context) const {
    Sampler input = input_->compile(context);
    std::vector<Sampler> samplers;
    if (thresholds_.size() == 1) {
        // parity: `SingleThresholdSampler` compiles only the first and the last function.
        samplers.push_back(functions_.front()->compile(context));
        samplers.push_back(functions_.back()->compile(context));
    } else {
        for (const Function& function : functions_) {
            samplers.push_back(function->compile(context));
        }
    }
    return std::make_shared<const IntervalSelectSampler>(std::move(input), thresholds_, std::move(samplers));
}

Function IntervalSelect::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    std::vector<Function> functions;
    functions.reserve(functions_.size());
    for (const Function& function : functions_) {
        functions.push_back(rewriteChild(function, rule, changed));
    }
    return changed ? std::make_shared<const IntervalSelect>(input, thresholds_, std::move(functions)) : self;
}

bool IntervalSelect::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::IntervalSelect) {
        return false;
    }
    const auto& that = static_cast<const IntervalSelect&>(other);
    if (!equal(input_, that.input_) || thresholds_.size() != that.thresholds_.size() ||
        functions_.size() != that.functions_.size()) {
        return false;
    }
    for (std::size_t i = 0; i < thresholds_.size(); ++i) {
        if (!sameFloat(thresholds_[i], that.thresholds_[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < functions_.size(); ++i) {
        if (!equal(functions_[i], that.functions_[i])) {
            return false;
        }
    }
    return true;
}

std::size_t IntervalSelect::hash() const {
    std::size_t seed = mix(15, input_->hash());
    for (const float threshold : thresholds_) {
        seed = mix(seed, hashFloat(threshold));
    }
    for (const Function& function : functions_) {
        seed = mix(seed, function->hash());
    }
    return seed;
}

} // namespace basalt::server::world::densityfunction::nodes
