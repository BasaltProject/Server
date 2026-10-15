#pragma once

#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration::stoneBlockTags {

using PathSet = std::set<std::string, std::less<>>;

/// parity: block tag `base_stone_overworld`.
[[nodiscard]] const PathSet& baseStoneOverworld();

/// parity: block tag `stone_ore_replaceables`.
[[nodiscard]] const PathSet& stoneOreReplaceables();

/// parity: block tag `deepslate_ore_replaceables`.
[[nodiscard]] const PathSet& deepslateOreReplaceables();

/// parity: `minecraft:height_specific_ore_replaceables`.
[[nodiscard]] const PathSet& heightSpecificOreReplaceables();

} // namespace basalt::server::world::terrain::decoration::stoneBlockTags
