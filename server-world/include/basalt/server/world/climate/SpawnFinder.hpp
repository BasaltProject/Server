#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/Sampler.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::world::climate {

/// parity: `Climate.SpawnFinder`: the column whose climate is closest to the dimension's
/// `spawn_target`, biased towards the origin. Two spirals, not a scan: the first walks out to 2048
/// in steps of 512, the second re-walks 512 in steps of 32 from wherever the first landed.
namespace spawnFinder {

/// parity: `worldgen/noise_settings/overworld.json`'s `spawn_target`.
[[nodiscard]] const std::vector<ParameterPoint>& overworldTarget();

/// The block column the world spawn is measured from, x and z only.
[[nodiscard]] std::pair<std::int32_t, std::int32_t> findSpawnPosition(const Sampler& sampler,
                                                                      const std::vector<ParameterPoint>& targets);

[[nodiscard]] std::pair<std::int32_t, std::int32_t> findSpawnPosition(const Sampler& sampler);

} // namespace spawnFinder

} // namespace basalt::server::world::climate
