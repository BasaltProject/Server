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

namespace treeDecoration {

using Candidate = std::function<void(Chunk&, random::RandomSource&, const DecorationPos&, random::RandomSource&)>;

struct Selector {
    std::vector<std::pair<float, Candidate>> entries;
    Candidate defaultCandidate;
    /// parity: `weighted_random_selector`.
    std::vector<std::pair<std::int32_t, Candidate>> weighted;
};

[[nodiscard]] Candidate tree(const tree::TreeConfiguration& config);
[[nodiscard]] Candidate fallen(const tree::FallenTreeConfiguration& config);
[[nodiscard]] Candidate hugeMushroom(hugeMushroomFeature::Kind kind);

[[nodiscard]] bool survivesPlacementPredicate(const Chunk& chunk, const DecorationPos& position,
                                              std::string_view sapling);

[[nodiscard]] const std::map<std::string, Selector, std::less<>>& selectors();

[[nodiscard]] const Selector& selector(std::string_view placedFeature);

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature,
             random::RandomSource& mossyCarpetRandom);

[[nodiscard]] BiomeVegetalDecoration countExtra(const Selector& selector, std::int32_t count, float chance,
                                                std::int32_t extra);

[[nodiscard]] BiomeVegetalDecoration sequenceOf(const Selector& selector, std::int32_t frequency);

[[nodiscard]] BiomeVegetalDecoration candidateSequenceCountExtra(std::vector<std::pair<float, Candidate>> entries,
                                                                 Candidate defaultCandidate, std::int32_t count,
                                                                 float chance, std::int32_t extra);

[[nodiscard]] BiomeVegetalDecoration single(const tree::TreeConfiguration& config, std::int32_t count, float chance,
                                            std::int32_t extra);

[[nodiscard]] BiomeVegetalDecoration fixed(const tree::TreeConfiguration& config, std::int32_t count);

[[nodiscard]] BiomeVegetalDecoration rarityChoice(const tree::TreeConfiguration& rareConfig, float rareChance,
                                                  const tree::TreeConfiguration& defaultConfig, std::int32_t rarity);

[[nodiscard]] BiomeVegetalDecoration providerCountChoice(const tree::TreeConfiguration& rareConfig, float rareChance,
                                                         const tree::TreeConfiguration& defaultConfig,
                                                         IntProvider count);

/// parity: `trees_water`.
[[nodiscard]] BiomeVegetalDecoration water();

} // namespace treeDecoration

} // namespace basalt::server::world::terrain::decoration
