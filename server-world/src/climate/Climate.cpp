#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/densityfunction/RandomState.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace basalt::server::world::climate {

namespace detail {
namespace {

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

Sampler::Sampler(densityfunction::RandomState& state, const densityfunction::Function& temperature,
                 const densityfunction::Function& humidity, const densityfunction::Function& continentalness,
                 const densityfunction::Function& erosion, const densityfunction::Function& depth,
                 const densityfunction::Function& weirdness)
    : temperature_(state.sampler(temperature)), humidity_(state.sampler(humidity)),
      continentalness_(state.sampler(continentalness)), erosion_(state.sampler(erosion)), depth_(state.sampler(depth)),
      weirdness_(state.sampler(weirdness)) {}

TargetPoint Sampler::sample(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                            densityfunction::SamplerContext* context) const {
    densityfunction::SamplerContext uncached{densityfunction::SamplerContext::Options{}};
    densityfunction::SamplerContext& active = context == nullptr ? uncached : *context;
    // parity: QuartPos.toBlock. A quart is four blocks, which is vanilla's biome grid resolution.
    const std::int32_t x = quartX << 2;
    const std::int32_t y = quartY << 2;
    const std::int32_t z = quartZ << 2;
    const float temperature = temperature_->sampleValue(active, x, y, z);
    const float humidity = humidity_->sampleValue(active, x, y, z);
    const float continentalness = continentalness_->sampleValue(active, x, y, z);
    const float erosion = erosion_->sampleValue(active, x, y, z);
    const float depth = depth_->sampleValue(active, x, y, z);
    const float weirdness = weirdness_->sampleValue(active, x, y, z);
    return target(temperature, humidity, continentalness, erosion, depth, weirdness);
}

} // namespace basalt::server::world::climate
