#pragma once

#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace basalt::server::world::spline {

/// The coordinate a spline samples along, plus the range it can take.
///
/// A function rather than a type parameter: vanilla parametrises the spline over the coordinate
/// type and threads it through every call site, which buys nothing here. What every caller actually
/// needs is one float out of whatever it is sampling.
template <typename C>
struct BoundedFloatFunction {
    std::function<float(const C&)> apply;
    float minValue = 0.0F;
    float maxValue = 0.0F;

    [[nodiscard]] static BoundedFloatFunction constant(float value) {
        return {[value](const C&) { return value; }, value, value};
    }
};

/// parity: `net.minecraft.util.CubicSpline`.
///
/// Either a constant, or a monotone Hermite spline through its locations, each paired with a child
/// spline (usually a constant, but they nest) and a tangent. Outside the first and last location it
/// extrapolates linearly along that endpoint's own derivative.
template <typename C>
class CubicSpline {
public:
    virtual ~CubicSpline() = default;
    CubicSpline(const CubicSpline&) = delete;
    CubicSpline& operator=(const CubicSpline&) = delete;
    CubicSpline(CubicSpline&&) = delete;
    CubicSpline& operator=(CubicSpline&&) = delete;

    [[nodiscard]] virtual float sample(const C& coordinate) const = 0;
    [[nodiscard]] virtual float minValue() const = 0;
    [[nodiscard]] virtual float maxValue() const = 0;

protected:
    CubicSpline() = default;
};

/// parity: `CubicSpline.Constant`, which ignores its coordinate.
template <typename C>
class ConstantSpline final : public CubicSpline<C> {
public:
    explicit ConstantSpline(float value) : value_(value) {}

    [[nodiscard]] float sample(const C&) const override { return value_; }

    [[nodiscard]] float minValue() const override { return value_; }

    [[nodiscard]] float maxValue() const override { return value_; }

private:
    float value_;
};

/// parity: `CubicSpline.Multipoint`. Its bounds are derived once, at construction.
template <typename C>
class MultipointSpline final : public CubicSpline<C> {
public:
    /// Refuses ragged or empty input: every caller builds one from constants, and a spline with no
    /// points has no value to give.
    [[nodiscard]] static std::unique_ptr<MultipointSpline> create(BoundedFloatFunction<C> coordinate,
                                                                  std::vector<float> locations,
                                                                  std::vector<std::unique_ptr<CubicSpline<C>>> values,
                                                                  std::vector<float> derivatives);

    [[nodiscard]] float sample(const C& coordinate) const override;

    [[nodiscard]] float minValue() const override { return minValue_; }

    [[nodiscard]] float maxValue() const override { return maxValue_; }

    [[nodiscard]] std::span<const float> locations() const noexcept { return locations_; }

    [[nodiscard]] std::span<const float> derivatives() const noexcept { return derivatives_; }

    MultipointSpline(BoundedFloatFunction<C> coordinate, std::vector<float> locations,
                     std::vector<std::unique_ptr<CubicSpline<C>>> values, std::vector<float> derivatives,
                     float minValue, float maxValue);

private:
    BoundedFloatFunction<C> coordinate_;
    std::vector<float> locations_;
    std::vector<std::unique_ptr<CubicSpline<C>>> values_;
    std::vector<float> derivatives_;
    float minValue_;
    float maxValue_;
};

/// A constant, as a spline. The common leaf of every built-in spline in the game.
template <typename C>
[[nodiscard]] std::unique_ptr<CubicSpline<C>> constantSpline(float value) {
    return std::make_unique<ConstantSpline<C>>(value);
}

} // namespace basalt::server::world::spline

#include <basalt/server/world/spline/CubicSpline.inl>
