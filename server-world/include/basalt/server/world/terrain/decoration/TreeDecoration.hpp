#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/HugeMushroomFeature.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/tree/FallenTreeFeature.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// Real per-biome tree placement, from the jar's own `VegetationPlacements` and
/// `BiomeDefaultFeatures`. A `RANDOM_SELECTOR` rolls its entries in declaration order, each with its
/// own draw, and stops at the first that lands. All 21 of the jar's selectors are here, keyed by
/// placed feature name: `trees_mangrove` is the jar's `mangrove_vegetation`, and
/// `trees_windswept_forest` shares `trees_windswept_hills`.
namespace treeDecoration {

using Candidate = std::function<void(Chunk&, random::RandomSource&, const DecorationPos&, random::RandomSource&)>;

/// One vanilla `RANDOM_SELECTOR`: the entries it rolls in order, and what it falls back to.
struct Selector {
    std::vector<std::pair<float, Candidate>> entries;
    Candidate defaultCandidate;
};

/// The configuration is held by reference: every caller passes one of the `treeConfigs` singletons,
/// which outlive any candidate built from them.
[[nodiscard]] Candidate tree(const tree::TreeConfiguration& config);
[[nodiscard]] Candidate fallen(const tree::FallenTreeConfiguration& config);
[[nodiscard]] Candidate hugeMushroom(hugeMushroomFeature::Kind kind);

/// The exact `would_survive` predicate shared by outer tree placements and nested selector entries.
[[nodiscard]] bool survivesPlacementPredicate(const Chunk& chunk, const DecorationPos& position,
                                              std::string_view sapling);

/// The named tree selectors, by their vanilla placed feature id. The biome table builds its
/// decorations from these rather than repeating the lists, so `/place feature` rolls the same one.
[[nodiscard]] const std::map<std::string, Selector, std::less<>>& selectors();

/// The named selector, for the biome table to build its own placement on.
[[nodiscard]] const Selector& selector(std::string_view placedFeature);

/// One roll of `placedFeature`'s selector, placed at `origin`: what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature,
             random::RandomSource& mossyCarpetRandom);

/// A real `treePlacement(countExtra(...))` frequency over a selector.
[[nodiscard]] BiomeVegetalDecoration countExtra(const Selector& selector, std::int32_t count, float chance,
                                                std::int32_t extra);

/// A real plain `CountPlacement.of(count)` frequency over a selector.
[[nodiscard]] BiomeVegetalDecoration sequenceOf(const Selector& selector, std::int32_t frequency);

[[nodiscard]] BiomeVegetalDecoration candidateSequenceCountExtra(std::vector<std::pair<float, Candidate>> entries,
                                                                 Candidate defaultCandidate, std::int32_t count,
                                                                 float chance, std::int32_t extra);

[[nodiscard]] BiomeVegetalDecoration single(const tree::TreeConfiguration& config, std::int32_t count, float chance,
                                            std::int32_t extra);

/// A real plain `CountPlacement.of(count)` frequency with no weighted "extra" roll.
[[nodiscard]] BiomeVegetalDecoration fixed(const tree::TreeConfiguration& config, std::int32_t count);

/// A two-entry `RANDOM_SELECTOR` behind one `RarityFilter`, such as `trees_meadow`.
[[nodiscard]] BiomeVegetalDecoration rarityChoice(const tree::TreeConfiguration& rareConfig, float rareChance,
                                                  const tree::TreeConfiguration& defaultConfig, std::int32_t rarity);

/// A two-entry selector whose attempt count comes from an arbitrary vanilla int provider.
[[nodiscard]] BiomeVegetalDecoration providerCountChoice(const tree::TreeConfiguration& rareConfig, float rareChance,
                                                         const tree::TreeConfiguration& defaultConfig,
                                                         IntProvider count);

/// parity: `trees_water`, weighted count 0x9/1x1 and fancy oak at 10% before the oak fallback.
[[nodiscard]] BiomeVegetalDecoration water();

} // namespace treeDecoration

} // namespace basalt::server::world::terrain::decoration
