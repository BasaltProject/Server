#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/Sampler.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::world::climate {

/// parity: `Climate.SpawnFinder`: the column whose climate is closest to the dimension's.
namespace spawnFinder {

/// parity: `worldgen/noise_settings/overworld.json`'s `spawn_target`.
[[nodiscard]] const std::vector<ParameterPoint>& overworldTarget();

[[nodiscard]] std::pair<std::int32_t, std::int32_t> findSpawnPosition(const Sampler& sampler,
                                                                      const std::vector<ParameterPoint>& targets);

[[nodiscard]] std::pair<std::int32_t, std::int32_t> findSpawnPosition(const Sampler& sampler);

} // namespace spawnFinder

} // namespace basalt::server::world::climate
