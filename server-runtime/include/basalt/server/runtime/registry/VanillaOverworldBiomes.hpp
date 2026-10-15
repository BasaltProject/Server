#pragma once

#include <basalt/server/runtime/registry/VanillaBiomes.hpp>

#include <vector>

namespace basalt::server::runtime::registry::vanillaBiomes {

[[nodiscard]] const std::vector<Entry>& overworldBiomesA();
[[nodiscard]] const std::vector<Entry>& overworldBiomesB();

[[nodiscard]] const std::vector<Entry>& netherBiomes();
[[nodiscard]] const std::vector<Entry>& endBiomes();

} // namespace basalt::server::runtime::registry::vanillaBiomes
