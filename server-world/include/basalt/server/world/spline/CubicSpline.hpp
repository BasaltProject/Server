#pragma once

#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
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

/// A child of a spline. Shared rather than owned outright because vanilla adds the *same* spline at
/// more than one point, and building it twice would be a second object with the same numbers.
template <typename C>
using SplineRef = std::shared_ptr<const CubicSpline<C>>;

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
    [[nodiscard]] static std::shared_ptr<MultipointSpline> create(BoundedFloatFunction<C> coordinate,
                                                                  std::vector<float> locations,
                                                                  std::vector<SplineRef<C>> values,
                                                                  std::vector<float> derivatives);

    [[nodiscard]] float sample(const C& coordinate) const override;

    [[nodiscard]] float minValue() const override { return minValue_; }

    [[nodiscard]] float maxValue() const override { return maxValue_; }

    [[nodiscard]] std::span<const float> locations() const noexcept { return locations_; }

    [[nodiscard]] std::span<const float> derivatives() const noexcept { return derivatives_; }

    MultipointSpline(BoundedFloatFunction<C> coordinate, std::vector<float> locations, std::vector<SplineRef<C>> values,
                     std::vector<float> derivatives, float minValue, float maxValue);

private:
    BoundedFloatFunction<C> coordinate_;
    std::vector<float> locations_;
    std::vector<SplineRef<C>> values_;
    std::vector<float> derivatives_;
    float minValue_;
    float maxValue_;
};

/// Builds a multipoint spline point by point, which is how every terrain spline is written.
///
/// Points must go in ascending order: the sampler binary-searches them, so an unordered one is not
/// a slower spline but a wrong one.
template <typename C>
class CubicSplineBuilder {
public:
    explicit CubicSplineBuilder(BoundedFloatFunction<C> coordinate) : coordinate_(std::move(coordinate)) {}

    CubicSplineBuilder& addPoint(float location, float value, float derivative = 0.0F) {
        return addPoint(location, std::make_shared<ConstantSpline<C>>(value), derivative);
    }

    CubicSplineBuilder& addPoint(float location, SplineRef<C> sampler, float derivative = 0.0F) {
        if (!locations_.empty() && location <= locations_.back()) {
            throw std::invalid_argument{"spline points must be added in ascending order"};
        }
        locations_.push_back(location);
        values_.push_back(std::move(sampler));
        derivatives_.push_back(derivative);
        return *this;
    }

    [[nodiscard]] SplineRef<C> build() {
        if (locations_.empty()) {
            throw std::invalid_argument{"a spline with no points has no value to give"};
        }
        return MultipointSpline<C>::create(std::move(coordinate_), std::move(locations_), std::move(values_),
                                           std::move(derivatives_));
    }

private:
    BoundedFloatFunction<C> coordinate_;
    std::vector<float> locations_;
    std::vector<SplineRef<C>> values_;
    std::vector<float> derivatives_;
};

/// A constant, as a spline. The common leaf of every built-in spline in the game.
template <typename C>
[[nodiscard]] SplineRef<C> constantSpline(float value) {
    return std::make_shared<ConstantSpline<C>>(value);
}

} // namespace basalt::server::world::spline

#include <basalt/server/world/spline/CubicSpline.inl>
