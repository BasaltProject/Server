#pragma once

#include <basalt/server/runtime/registry/VanillaBiomes.hpp>

#include <vector>

namespace basalt::server::runtime::registry::vanillaBiomes {

/// The overworld biome entries, split in two for file size. Order matters: it is part of the
/// sequence the whole registry goes out in, and that sequence is the network id a chunk's biome
/// palette references. The first runs from `plains` through `wooded_badlands`.
[[nodiscard]] const std::vector<Entry>& overworldBiomesA();
[[nodiscard]] const std::vector<Entry>& overworldBiomesB();

/// The nether and the end, both built from small factories because their entries repeat most of
/// their fields.
[[nodiscard]] const std::vector<Entry>& netherBiomes();
[[nodiscard]] const std::vector<Entry>& endBiomes();

} // namespace basalt::server::runtime::registry::vanillaBiomes
