#include <basalt/server/world/noise/Interval.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>

#include <cmath>

namespace basalt::server::world::noise {
namespace {

using javaMath::fmax;
using javaMath::fmin;

[[nodiscard]] float mulBound(float left, float right) noexcept {
    return left != 0.0F && right != 0.0F ? left * right : 0.0F;
}

[[nodiscard]] float powf(float base, float exponent) noexcept {
    return static_cast<float>(javaMath::pow(static_cast<double>(base), static_cast<double>(exponent)));
}

[[nodiscard]] Interval encapsulatingWith(const Interval& first, float second) noexcept {
    if (std::isnan(second)) {
        return first;
    }
    return first.isNaI() ? Interval::ofExact(second)
                         : Interval::of(fmin(first.min(), second), fmax(first.max(), second));
}

[[nodiscard]] float lerpFiniteBound(float alpha, float first, float second) noexcept {
    return first + mulBound(alpha, second - first);
}

[[nodiscard]] float lerpInfiniteBound(float alpha, float first, float second) noexcept {
    const float firstPart = mulBound(1.0F - alpha, first);
    const float secondPart = mulBound(alpha, second);
    if (std::isinf(firstPart) && std::isinf(secondPart)) {
        if (alpha <= 0.0F) {
            return second > first ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
        }
        if (alpha >= 1.0F) {
            return second > first ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
        }
        return std::numeric_limits<float>::quiet_NaN();
    }
    return firstPart + secondPart;
}

} // namespace

Interval Interval::encapsulating(std::initializer_list<Interval> intervals) noexcept {
    float min = kInfinity;
    float max = -kInfinity;
    for (const Interval& interval : intervals) {
        if (!interval.isNaI()) {
            min = fmin(interval.min_, min);
            max = fmax(interval.max_, max);
        }
    }
    return max < min ? notAnInterval() : of(min, max);
}

Interval Interval::encapsulating(float first, float second) noexcept {
    if (std::isnan(first) && std::isnan(second)) {
        return notAnInterval();
    }
    if (std::isnan(first)) {
        return ofExact(second);
    }
    return std::isnan(second) ? ofExact(first) : of(fmin(first, second), fmax(first, second));
}

Interval Interval::add(const Interval& left, const Interval& right) noexcept {
    const float min = left.min_ + right.min_;
    const float max = left.max_ + right.max_;
    return !std::isnan(min) && !std::isnan(max) ? of(min, max) : notAnInterval();
}

Interval Interval::sub(const Interval& left, const Interval& right) noexcept {
    const float min = left.min_ - right.max_;
    const float max = left.max_ - right.min_;
    return !std::isnan(min) && !std::isnan(max) ? of(min, max) : notAnInterval();
}

Interval Interval::mul(const Interval& left, const Interval& right) noexcept {
    if (left.isNaI() || right.isNaI()) {
        return notAnInterval();
    }
    const float minMin = mulBound(left.min_, right.min_);
    const float minMax = mulBound(left.min_, right.max_);
    const float maxMin = mulBound(left.max_, right.min_);
    const float maxMax = mulBound(left.max_, right.max_);
    return of(fmin(fmin(minMin, minMax), fmin(maxMin, maxMax)), fmax(fmax(minMin, minMax), fmax(maxMin, maxMax)));
}

Interval Interval::reciprocal(const Interval& input) noexcept {
    if (input.isNaI() || (input.min_ == 0.0F && input.max_ == 0.0F)) {
        return notAnInterval();
    }
    if (!input.contains(0.0F)) {
        return of(1.0F / input.max_, 1.0F / input.min_);
    }
    if (input.max_ == 0.0F) {
        return of(-kInfinity, 1.0F / input.min_);
    }
    return input.min_ == 0.0F ? of(1.0F / input.max_, kInfinity) : infinite();
}

Interval Interval::min(const Interval& left, const Interval& right) noexcept {
    return !left.isNaI() && !right.isNaI() ? of(fmin(left.min_, right.min_), fmin(left.max_, right.max_))
                                           : notAnInterval();
}

Interval Interval::max(const Interval& left, const Interval& right) noexcept {
    return !left.isNaI() && !right.isNaI() ? of(fmax(left.min_, right.min_), fmax(left.max_, right.max_))
                                           : notAnInterval();
}

Interval Interval::clamp(const Interval& input, float min, float max) noexcept {
    if (input.isNaI()) {
        return notAnInterval();
    }
    if (input.min_ >= max) {
        return of(max, max);
    }
    return input.max_ <= min ? of(min, min) : of(fmax(input.min_, min), fmin(input.max_, max));
}

Interval Interval::abs(const Interval& input) noexcept {
    if (input.isNaI()) {
        return notAnInterval();
    }
    const float max = fmax(std::fabs(input.min_), std::fabs(input.max_));
    return input.contains(0.0F) ? of(0.0F, max) : of(fmin(std::fabs(input.min_), std::fabs(input.max_)), max);
}

Interval Interval::square(const Interval& input) noexcept {
    if (input.isNaI()) {
        return notAnInterval();
    }
    const float low = input.min_ * input.min_;
    const float high = input.max_ * input.max_;
    const float max = fmax(low, high);
    return input.contains(0.0F) ? of(0.0F, max) : of(fmin(low, high), max);
}

Interval Interval::powScalar(float base, const Interval& exponent) noexcept {
    if (std::isnan(base) || exponent.isNaI()) {
        return notAnInterval();
    }
    if (exponent.min_ == exponent.max_) {
        const float value = powf(base, exponent.min_);
        return std::isnan(value) ? notAnInterval() : ofExact(value);
    }
    if (base == 0.0F) {
        Interval zeroBase = notAnInterval();
        if (exponent.contains(0.0F)) {
            if (exponent.max_ == 0.0F) {
                zeroBase = of(1.0F, kInfinity);
            } else {
                zeroBase = exponent.min_ == 0.0F ? of(0.0F, 1.0F) : of(0.0F, kInfinity);
            }
        } else {
            zeroBase = exponent.max_ < 0.0F ? ofExact(kInfinity) : ofExact(0.0F);
        }
        return mul(zeroBase, ofExact(std::copysign(1.0F, base)));
    }
    if (base == 1.0F) {
        return ofExact(1.0F);
    }
    if (base > 0.0F) {
        if (std::isfinite(exponent.min_) && std::isfinite(exponent.max_)) {
            return encapsulating(powf(base, exponent.min_), powf(base, exponent.max_));
        }
        if (std::isinf(exponent.min_) && std::isinf(exponent.max_)) {
            return of(0.0F, kInfinity);
        }
        if (std::isinf(exponent.min_)) {
            return base < 1.0F ? of(powf(base, exponent.max_), kInfinity) : of(0.0F, powf(base, exponent.max_));
        }
        return base < 1.0F ? of(0.0F, powf(base, exponent.min_)) : of(powf(base, exponent.min_), kInfinity);
    }
    const auto minInt = static_cast<float>(std::ceil(static_cast<double>(exponent.min_)));
    const auto maxInt = static_cast<float>(std::floor(static_cast<double>(exponent.max_)));
    if (maxInt < minInt) {
        return notAnInterval();
    }
    const float baseToMin = powf(base, minInt);
    const float baseToMax = powf(base, maxInt);
    Interval result = encapsulating(baseToMin, baseToMax);
    if (std::isinf(minInt)) {
        result = encapsulatingWith(result, -baseToMin);
    } else if (minInt + 1.0F < maxInt) {
        result = encapsulatingWith(result, powf(base, minInt + 1.0F));
    }
    if (std::isinf(maxInt)) {
        result = encapsulatingWith(result, -baseToMax);
    } else if (maxInt - 1.0F > minInt) {
        result = encapsulatingWith(result, powf(base, maxInt - 1.0F));
    }
    return result;
}

Interval Interval::pow(const Interval& base, const Interval& exponent) noexcept {
    if (base.isNaI() || exponent.isNaI()) {
        return notAnInterval();
    }
    if (base.min_ == base.max_) {
        return powScalar(base.min_, exponent);
    }
    const Interval low = powScalar(base.min_, exponent);
    const Interval high = powScalar(base.max_, exponent);
    Interval result = encapsulating({low, high});
    if (base.contains(0.0F)) {
        if (base.max_ > 0.0F) {
            result = encapsulating({result, powScalar(0.0F, exponent)});
        }
        if (base.min_ < 0.0F) {
            result = encapsulating({result, powScalar(-0.0F, exponent)});
        }
    }
    return result;
}

Interval Interval::log(const Interval& input) noexcept {
    if (input.max_ < 0.0F) {
        return notAnInterval();
    }
    return mapMonotonic(max(input, ofExact(0.0F)),
                        [](float x) { return static_cast<float>(std::log(static_cast<double>(x))); });
}

Interval Interval::mapMonotonic(const Interval& input, const std::function<float(float)>& op) noexcept {
    if (input.isNaI()) {
        return notAnInterval();
    }
    const float mappedMin = op(input.min_);
    const float mappedMax = op(input.max_);
    if (std::isnan(mappedMin) || std::isnan(mappedMax)) {
        return notAnInterval();
    }
    return of(fmin(mappedMin, mappedMax), fmax(mappedMin, mappedMax));
}

Interval Interval::lerp(const Interval& alpha, const Interval& first, const Interval& second) noexcept {
    if (alpha.isNaI() || first.isNaI() || second.isNaI()) {
        return notAnInterval();
    }
    return encapsulating({lerp(alpha, first.min_, second.min_), lerp(alpha, first.max_, second.min_),
                          lerp(alpha, first.min_, second.max_), lerp(alpha, first.max_, second.max_)});
}

Interval Interval::lerp(const Interval& alpha, float first, float second) noexcept {
    if (alpha.isNaI() || std::isnan(first) || std::isnan(second)) {
        return notAnInterval();
    }
    if (std::isfinite(first) && std::isfinite(second)) {
        return encapsulating(lerpFiniteBound(alpha.min_, first, second), lerpFiniteBound(alpha.max_, first, second));
    }
    if (first == second) {
        return ofExact(first);
    }
    const float newMin = lerpInfiniteBound(alpha.min_, first, second);
    const float newMax = lerpInfiniteBound(alpha.max_, first, second);
    return !std::isnan(newMin) && !std::isnan(newMax) ? encapsulating(newMin, newMax) : notAnInterval();
}

Interval Interval::sign(const Interval& input) noexcept {
    if (input.isNaI()) {
        return notAnInterval();
    }
    if (input.min_ == input.max_) {
        return ofExact(javaMath::signum(input.min_));
    }
    if (input.contains(0.0F)) {
        if (input.min_ == 0.0F) {
            return of(0.0F, 1.0F);
        }
        return input.max_ == 0.0F ? of(-1.0F, 0.0F) : of(-1.0F, 1.0F);
    }
    return ofExact(input.min_ > 0.0F ? 1.0F : -1.0F);
}

} // namespace basalt::server::world::noise
