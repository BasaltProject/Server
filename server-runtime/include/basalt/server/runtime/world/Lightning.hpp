#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `ServerLevel.tickChunk`.
namespace lightning {

inline constexpr std::int32_t kStrikeOdds = 100000;

inline constexpr std::int32_t kRodRange = 128;

inline constexpr std::int32_t kRodTicks = 8;

[[nodiscard]] const wire::common::Identifier& rod();

[[nodiscard]] const wire::common::Identifier& bolt();

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> targetFor(WorldState& world, std::int32_t x, std::int32_t z);

/// parity: `LightningBolt.powerLightningRod`.
void strike(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool lightFires,
            random::RandomSource& random, std::vector<BlockChange>& changes);

/// parity: `LightningBolt.spawnFire`.
void spawnFire(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t additionalSources,
               random::RandomSource& random, std::vector<BlockChange>& changes);

[[nodiscard]] std::optional<BlockChange> releaseRod(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: the roll a chunk makes while the sky is thundering.
[[nodiscard]] bool shouldStrike(random::RandomSource& random);

} // namespace lightning

} // namespace basalt::server::runtime::world
