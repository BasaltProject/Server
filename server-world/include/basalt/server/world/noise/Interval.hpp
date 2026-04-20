#pragma once

#include <functional>
#include <initializer_list>
#include <limits>

namespace basalt::server::world::noise {

/// parity: `net.minecraft.util.Interval`.
class Interval {
public:
    [[nodiscard]] static Interval notAnInterval() noexcept { return Interval{kNaN, kNaN, true}; }

    [[nodiscard]] static Interval infinite() noexcept { return Interval{-kInfinity, kInfinity, false}; }

    [[nodiscard]] static Interval of(float min, float max) noexcept { return Interval{min, max, false}; }

    [[nodiscard]] static Interval ofSymmetric(float range) noexcept { return of(-range, range); }

    [[nodiscard]] static Interval ofExact(float value) noexcept { return of(value, value); }

    [[nodiscard]] static Interval encapsulating(std::initializer_list<Interval> intervals) noexcept;

    [[nodiscard]] static Interval encapsulating(float first, float second) noexcept;

    [[nodiscard]] static Interval add(const Interval& left, const Interval& right) noexcept;

    [[nodiscard]] static Interval sub(const Interval& left, const Interval& right) noexcept;

    [[nodiscard]] static Interval mul(const Interval& left, const Interval& right) noexcept;

    [[nodiscard]] static Interval reciprocal(const Interval& input) noexcept;

    [[nodiscard]] static Interval div(const Interval& left, const Interval& right) noexcept {
        return mul(left, reciprocal(right));
    }

    [[nodiscard]] static Interval min(const Interval& left, const Interval& right) noexcept;

    [[nodiscard]] static Interval max(const Interval& left, const Interval& right) noexcept;

    [[nodiscard]] static Interval clamp(const Interval& input, float min, float max) noexcept;

    [[nodiscard]] static Interval abs(const Interval& input) noexcept;

    [[nodiscard]] static Interval square(const Interval& input) noexcept;

    [[nodiscard]] static Interval pow(const Interval& base, const Interval& exponent) noexcept;

    [[nodiscard]] static Interval log(const Interval& input) noexcept;

    [[nodiscard]] static Interval mapMonotonic(const Interval& input, const std::function<float(float)>& op) noexcept;

    [[nodiscard]] static Interval lerp(const Interval& alpha, const Interval& first, const Interval& second) noexcept;

    [[nodiscard]] static Interval lerp(const Interval& alpha, float first, float second) noexcept;

    [[nodiscard]] static Interval sign(const Interval& input) noexcept;

    [[nodiscard]] bool isNaI() const noexcept { return nai_; }

    [[nodiscard]] float min() const noexcept { return min_; }

    [[nodiscard]] float max() const noexcept { return max_; }

    [[nodiscard]] bool contains(float value) const noexcept { return value >= min_ && value <= max_; }

private:
    static constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    static constexpr float kInfinity = std::numeric_limits<float>::infinity();

    Interval(float min, float max, bool nai) noexcept : min_(min), max_(max), nai_(nai) {}

    [[nodiscard]] static Interval powScalar(float base, const Interval& exponent) noexcept;

    float min_;
    float max_;
    bool nai_;
};

} // namespace basalt::server::world::noise
