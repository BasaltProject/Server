#include <basalt/server/world/climate/Climate.hpp>

#include <atomic>
#include <stdexcept>
#include <vector>

namespace basalt::server::world::climate {

namespace detail {
namespace {

/// One entry per tree, per thread. A vector rather than a map: slots are dense and handed out in
/// order, and this is read on every climate lookup.
thread_local std::vector<const void*> leaves;
std::atomic<std::size_t> slots{0};

} // namespace

const void* lastLeaf(std::size_t slot) { return slot < leaves.size() ? leaves[slot] : nullptr; }

void setLastLeaf(std::size_t slot, const void* leaf) {
    if (slot >= leaves.size()) {
        leaves.resize(slot + 1, nullptr);
    }
    leaves[slot] = leaf;
}

std::size_t nextTreeSlot() { return slots++; }

} // namespace detail

Parameter parameterSpan(float min, float max) {
    if (min > max) {
        throw std::invalid_argument{"a climate parameter's minimum is above its maximum"};
    }
    return {quantizeCoord(min), quantizeCoord(max)};
}

TargetPoint target(float temperature, float humidity, float continentalness, float erosion, float depth,
                   float weirdness) {
    return {quantizeCoord(temperature), quantizeCoord(humidity), quantizeCoord(continentalness),
            quantizeCoord(erosion),     quantizeCoord(depth),    quantizeCoord(weirdness)};
}

std::int64_t ParameterPoint::fitness(const TargetPoint& point) const {
    const auto square = [](std::int64_t value) { return value * value; };
    return square(temperature.distance(point.temperature)) + square(humidity.distance(point.humidity)) +
           square(continentalness.distance(point.continentalness)) + square(erosion.distance(point.erosion)) +
           square(depth.distance(point.depth)) + square(weirdness.distance(point.weirdness)) + square(offset);
}

std::array<Parameter, kDimensions> ParameterPoint::parameterSpace() const {
    return {temperature, humidity, continentalness, erosion, depth, weirdness, Parameter{offset, offset}};
}

ParameterPoint parameters(float temperature, float humidity, float continentalness, float erosion, float depth,
                          float weirdness, float offset) {
    return {parameterPointAt(temperature), parameterPointAt(humidity), parameterPointAt(continentalness),
            parameterPointAt(erosion),     parameterPointAt(depth),    parameterPointAt(weirdness),
            quantizeCoord(offset)};
}

} // namespace basalt::server::world::climate
