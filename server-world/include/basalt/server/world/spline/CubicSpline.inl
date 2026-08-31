#pragma once

#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace basalt::server::world::spline {
namespace detail {

/// The value carried straight out past an endpoint, along that endpoint's own tangent.
[[nodiscard]] inline float linearExtend(float input, std::span<const float> locations, float value,
                                        std::span<const float> derivatives, std::size_t index) {
    const float derivative = derivatives[index];
    return derivative == 0.0F ? value : value + derivative * (input - locations[index]);
}

/// The interval `input` falls in, or -1 before the first location.
[[nodiscard]] inline int findIntervalStart(std::span<const float> locations, float input) {
    return wire::common::mth::binarySearch(0, static_cast<int>(locations.size()),
                                           [&](int at) { return input < locations[static_cast<std::size_t>(at)]; }) -
           1;
}

} // namespace detail

template <typename C>
MultipointSpline<C>::MultipointSpline(BoundedFloatFunction<C> coordinate, std::vector<float> locations,
                                      std::vector<std::unique_ptr<CubicSpline<C>>> values,
                                      std::vector<float> derivatives, float minValue, float maxValue)
    : coordinate_(std::move(coordinate)), locations_(std::move(locations)), values_(std::move(values)),
      derivatives_(std::move(derivatives)), minValue_(minValue), maxValue_(maxValue) {}

template <typename C>
float MultipointSpline<C>::sample(const C& coordinate) const {
    const float input = coordinate_.apply(coordinate);
    const std::size_t lastIndex = locations_.size() - 1;
    const int start = detail::findIntervalStart(locations_, input);
    if (start < 0) {
        return detail::linearExtend(input, locations_, values_.front()->sample(coordinate), derivatives_, 0);
    }
    if (static_cast<std::size_t>(start) == lastIndex) {
        return detail::linearExtend(input, locations_, values_[lastIndex]->sample(coordinate), derivatives_, lastIndex);
    }

    const auto at = static_cast<std::size_t>(start);
    const float x1 = locations_[at];
    const float x2 = locations_[at + 1];
    const float t = (input - x1) / (x2 - x1);
    const float d1 = derivatives_[at];
    const float d2 = derivatives_[at + 1];
    const float y1 = values_[at]->sample(coordinate);
    const float y2 = values_[at + 1]->sample(coordinate);
    const float a = d1 * (x2 - x1) - (y2 - y1);
    const float b = -d2 * (x2 - x1) + (y2 - y1);
    return wire::common::mth::lerp(t, y1, y2) + t * (1.0F - t) * wire::common::mth::lerp(t, a, b);
}

template <typename C>
std::unique_ptr<MultipointSpline<C>>
MultipointSpline<C>::create(BoundedFloatFunction<C> coordinate, std::vector<float> locations,
                            std::vector<std::unique_ptr<CubicSpline<C>>> values, std::vector<float> derivatives) {
    if (locations.size() != values.size() || locations.size() != derivatives.size()) {
        throw std::invalid_argument{"a spline's locations, values and derivatives must be the same length"};
    }
    if (locations.empty()) {
        throw std::invalid_argument{"a spline with no points has no value to give"};
    }

    const std::size_t lastIndex = locations.size() - 1;
    float minValue = std::numeric_limits<float>::infinity();
    float maxValue = -std::numeric_limits<float>::infinity();

    if (coordinate.minValue < locations[0]) {
        const float edge1 = detail::linearExtend(coordinate.minValue, locations, values[0]->minValue(), derivatives, 0);
        const float edge2 = detail::linearExtend(coordinate.minValue, locations, values[0]->maxValue(), derivatives, 0);
        minValue = std::min(minValue, std::min(edge1, edge2));
        maxValue = std::max(maxValue, std::max(edge1, edge2));
    }
    if (coordinate.maxValue > locations[lastIndex]) {
        const float edge1 =
            detail::linearExtend(coordinate.maxValue, locations, values[lastIndex]->minValue(), derivatives, lastIndex);
        const float edge2 =
            detail::linearExtend(coordinate.maxValue, locations, values[lastIndex]->maxValue(), derivatives, lastIndex);
        minValue = std::min(minValue, std::min(edge1, edge2));
        maxValue = std::max(maxValue, std::max(edge1, edge2));
    }
    for (const std::unique_ptr<CubicSpline<C>>& value : values) {
        minValue = std::min(minValue, value->minValue());
        maxValue = std::max(maxValue, value->maxValue());
    }
    for (std::size_t i = 0; i < lastIndex; ++i) {
        const float xDiff = locations[i + 1] - locations[i];
        const float min1 = values[i]->minValue();
        const float max1 = values[i]->maxValue();
        const float min2 = values[i + 1]->minValue();
        const float max2 = values[i + 1]->maxValue();
        const float d1 = derivatives[i];
        const float d2 = derivatives[i + 1];
        if (d1 == 0.0F && d2 == 0.0F) {
            continue;
        }
        const float p1 = d1 * xDiff;
        const float p2 = d2 * xDiff;
        const float minLerp1 = std::min(min1, min2);
        const float maxLerp1 = std::max(max1, max2);
        const float minA = p1 - max2 + min1;
        const float maxA = p1 - min2 + max1;
        const float minB = -p2 + min2 - max1;
        const float maxB = -p2 + max2 - min1;
        minValue = std::min(minValue, minLerp1 + 0.25F * std::min(minA, minB));
        maxValue = std::max(maxValue, maxLerp1 + 0.25F * std::max(maxA, maxB));
    }

    return std::make_unique<MultipointSpline<C>>(std::move(coordinate), std::move(locations), std::move(values),
                                                 std::move(derivatives), minValue, maxValue);
}

} // namespace basalt::server::world::spline
