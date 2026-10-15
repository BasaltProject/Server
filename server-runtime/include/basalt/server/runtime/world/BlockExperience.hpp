#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::world {

namespace blockExperience {

[[nodiscard]] std::int32_t onBreak(const wire::common::Identifier& block, bool silkTouch, random::RandomSource& random);

} // namespace blockExperience

} // namespace basalt::server::runtime::world
