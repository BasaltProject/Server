#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::world {

/// What a block leaves as experience when a player breaks it with the right tool.
namespace blockExperience {

/// parity: `spawnAfterBreak` of `DropExperienceBlock`, `RedStoneOreBlock`, the sculk blocks and
/// `SpawnerBlock`. `silkTouch` is the one enchantment with a `block_experience` effect, which sets
/// it to zero; the spawner pops its own and does not ask.
[[nodiscard]] std::int32_t onBreak(const wire::common::Identifier& block, bool silkTouch, random::RandomSource& random);

} // namespace blockExperience

} // namespace basalt::server::runtime::world
