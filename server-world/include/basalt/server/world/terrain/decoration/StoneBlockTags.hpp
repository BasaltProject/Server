#pragma once

#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration::stoneBlockTags {

/// The three block tags worldgen keeps asking about, in one place. They were written out four times
/// between the ore configs, the deferred write rules and the two cave decorations, which is the shape
/// every configuration bug this generator has had so far arrived in.
using PathSet = std::set<std::string, std::less<>>;

/// parity: block tag `base_stone_overworld`.
[[nodiscard]] const PathSet& baseStoneOverworld();

/// parity: block tag `stone_ore_replaceables`.
[[nodiscard]] const PathSet& stoneOreReplaceables();

/// parity: block tag `deepslate_ore_replaceables`.
[[nodiscard]] const PathSet& deepslateOreReplaceables();

/// parity: `minecraft:height_specific_ore_replaceables`: tuff, which since 26.3 takes the stone or the
/// deepslate ore by height rather than by tag.
[[nodiscard]] const PathSet& heightSpecificOreReplaceables();

} // namespace basalt::server::world::terrain::decoration::stoneBlockTags
