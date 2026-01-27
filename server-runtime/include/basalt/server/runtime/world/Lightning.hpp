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

/// What a storm drops on the world.
///
/// parity: `ServerLevel.tickChunk`'s thunder half over `LightningBolt`. A rod within 128 blocks
/// takes the strike instead of whatever was under it, and being struck powers it for eight ticks.
/// Fires are lit only on normal and hard, which is why a peaceful thunderstorm burns nothing.
namespace lightning {

/// parity: the one in a hundred thousand a chunk rolls each tick while it thunders.
inline constexpr std::int32_t kStrikeOdds = 100000;

/// parity: `findLightningRod`, which looks 128 blocks around the target.
inline constexpr std::int32_t kRodRange = 128;

/// parity: `LightningRodBlock.onLightningStrike`, eight ticks of signal.
inline constexpr std::int32_t kRodTicks = 8;

[[nodiscard]] const wire::common::Identifier& rod();

/// The bolt itself, which is an entity: `/summon minecraft:lightning_bolt` makes one and a storm
/// makes one, and both do the same thing on their first turn.
[[nodiscard]] const wire::common::Identifier& bolt();

/// Where a strike aimed at this column actually lands: a rod nearby, or the top of the column.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> targetFor(WorldState& world, std::int32_t x, std::int32_t z);

/// parity: `LightningBolt.powerLightningRod` and `spawnFire`: the rod it hit answers, and on the
/// harder difficulties the ground it hit catches.
void strike(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool lightFires,
            random::RandomSource& random, std::vector<BlockChange>& changes);

/// The scheduled half: the rod lets its signal go eight ticks after it was struck.
[[nodiscard]] std::optional<BlockChange> releaseRod(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: the roll a chunk makes while the sky is thundering.
[[nodiscard]] bool shouldStrike(random::RandomSource& random);

} // namespace lightning

} // namespace basalt::server::runtime::world
