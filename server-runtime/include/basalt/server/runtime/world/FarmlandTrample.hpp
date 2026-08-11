#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>

namespace basalt::server::runtime::config {
struct PurpurRules;
}

namespace basalt::server::runtime::world {

/// parity: `FarmlandBlock.fallOn`.
namespace farmlandTrample {

struct Faller {
    double fallDistance = 0.0;
    float width = 0.6F;
    float height = 1.8F;
    bool isPlayer = false;
    std::int32_t featherFalling = 0;
};

[[nodiscard]] bool tramples(const config::PurpurRules& rules, const Faller& faller, random::RandomSource& random);

} // namespace farmlandTrample

} // namespace basalt::server::runtime::world
