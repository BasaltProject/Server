#pragma once

#include <functional>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {

/// parity: the `#minecraft:is_overworld` biome tag as the decoration steps read it.
[[nodiscard]] const std::set<std::string, std::less<>>& overworldBiomePaths();

} // namespace basalt::server::world::terrain::decoration
