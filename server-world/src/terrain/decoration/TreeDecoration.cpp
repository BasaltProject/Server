#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kGroveScanSteps = 8;

[[nodiscard]] std::string pathOf(const Chunk& chunk, const DecorationPos& position) {
    const wire::common::Identifier* block =
        registry::blockStateDefinitions::blockOf(chunk.decorationBlockStateId(position.x, position.y, position.z));
    return block == nullptr ? std::string{} : block->path();
}

/// Nested `pine_on_snow` and `spruce_on_snow`: scan up through at most eight powder snow blocks.
[[nodiscard]] std::optional<DecorationPos> grovePosition(const Chunk& chunk, const DecorationPos& origin) {
    static const std::set<std::string, std::less<>> supportPaths = {"snow_block", "powder_snow"};
    const ChunkHeight bounds = chunk.verticalBounds();
    DecorationPos position = origin;
    for (std::int32_t steps = 0; steps < kGroveScanSteps && pathOf(chunk, position) == "powder_snow"; ++steps) {
        if (position.y == bounds.maxY()) {
            return std::nullopt;
        }
        position.y++;
    }
    if (pathOf(chunk, position) == "powder_snow") {
        return std::nullopt;
    }
    const DecorationPos below{.x = position.x, .y = position.y - 1, .z = position.z};
    if (!supportPaths.contains(pathOf(chunk, below))) {
        return std::nullopt;
    }
    return position;
}

[[nodiscard]] treeDecoration::Candidate checked(treeDecoration::Candidate candidate, std::string sapling) {
    return [candidate = std::move(candidate), sapling = std::move(sapling)](Chunk& chunk, random::RandomSource& random,
                                                                            const DecorationPos& position,
                                                                            random::RandomSource& mossyCarpetRandom) {
        if (treeDecoration::survivesPlacementPredicate(chunk, position, sapling)) {
            candidate(chunk, random, position, mossyCarpetRandom);
        }
    };
}

[[nodiscard]] treeDecoration::Candidate checkedTree(const tree::TreeConfiguration& config, std::string sapling) {
    return checked(treeDecoration::tree(config), std::move(sapling));
}

[[nodiscard]] treeDecoration::Candidate checkedFallen(const tree::FallenTreeConfiguration& config,
                                                      std::string sapling) {
    return checked(treeDecoration::fallen(config), std::move(sapling));
}

[[nodiscard]] treeDecoration::Candidate onSnow(const tree::TreeConfiguration& config) {
    return [candidate = treeDecoration::tree(config)](Chunk& chunk, random::RandomSource& random,
                                                      const DecorationPos& position,
                                                      random::RandomSource& mossyCarpetRandom) {
        const std::optional<DecorationPos> grove = grovePosition(chunk, position);
        if (grove.has_value()) {
            candidate(chunk, random, *grove, mossyCarpetRandom);
        }
    };
}

/// Both windswept selectors are the same roll; only their counts differ, and the count is placement.
[[nodiscard]] treeDecoration::Selector windsweptSelector() {
    return {.entries = {{0.008325F, checkedFallen(tree::fallenTreeConfigs::spruce(), "spruce_sapling")},
                        {0.666F, checkedTree(tree::treeConfigs::spruce(), "spruce_sapling")},
                        {0.1F, checkedTree(tree::treeConfigs::fancyOak(), "oak_sapling")},
                        {0.0125F, checkedFallen(tree::fallenTreeConfigs::oak(), "oak_sapling")}},
            .defaultCandidate = checkedTree(tree::treeConfigs::oak(), "oak_sapling")};
}

[[nodiscard]] const treeDecoration::Candidate*
pickWeighted(random::RandomSource& random,
             const std::vector<std::pair<std::int32_t, treeDecoration::Candidate>>& entries) {
    std::int32_t total = 0;
    for (const auto& [weight, candidate] : entries) {
        total += weight;
    }
    std::int32_t roll = random.nextInt(total);
    for (const auto& [weight, candidate] : entries) {
        if (roll < weight) {
            return &candidate;
        }
        roll -= weight;
    }
    return &entries.back().second;
}

[[nodiscard]] const treeDecoration::Candidate* pick(random::RandomSource& random,
                                                    const treeDecoration::Selector& selector);

[[nodiscard]] const treeDecoration::Candidate*
pickSequence(random::RandomSource& random, const std::vector<std::pair<float, treeDecoration::Candidate>>& entries,
             const treeDecoration::Candidate& defaultCandidate) {
    for (const auto& [chance, candidate] : entries) {
        if (random.nextFloat() < chance) {
            return &candidate;
        }
    }
    return &defaultCandidate;
}

[[nodiscard]] const treeDecoration::Candidate* pick(random::RandomSource& random,
                                                    const treeDecoration::Selector& selector) {
    if (!selector.weighted.empty()) {
        return pickWeighted(random, selector.weighted);
    }
    return pickSequence(random, selector.entries, selector.defaultCandidate);
}

/// parity: the shared tree placement pipeline, in square, above water, on the ocean floor.
void place(Chunk& chunk, random::RandomSource& random, PlacementModifier frequency,
           const std::function<const treeDecoration::Candidate*(random::RandomSource&)>& pick) {
    const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    const std::vector<PlacementModifier> modifiers{std::move(frequency), inSquarePlacement(),
                                                   surfaceWaterDepthFilter(0),
                                                   heightmapPlacement(HeightmapType::OceanFloor)};
    placeFrom(chunk, random, origin, modifiers, [&](const DecorationPos& position) {
        const treeDecoration::Candidate* candidate = pick(random);
        if (candidate != nullptr && *candidate) {
            (*candidate)(chunk, random, position, random);
        }
    });
}

} // namespace

treeDecoration::Candidate treeDecoration::tree(const tree::TreeConfiguration& config) {
    return [&config](Chunk& chunk, random::RandomSource& random, const DecorationPos& position,
                     random::RandomSource& mossyCarpetRandom) {
        tree::treeFeature::place(chunk, random, position.x, position.y, position.z, config, mossyCarpetRandom);
    };
}

treeDecoration::Candidate treeDecoration::fallen(const tree::FallenTreeConfiguration& config) {
    return [&config](Chunk& chunk, random::RandomSource& random, const DecorationPos& position, random::RandomSource&) {
        tree::fallenTreeFeature::place(chunk, random, position.x, position.y, position.z, config);
    };
}

treeDecoration::Candidate treeDecoration::hugeMushroom(const hugeMushroomFeature::Kind kind) {
    return [kind](Chunk& chunk, random::RandomSource& random, const DecorationPos& position, random::RandomSource&) {
        hugeMushroomFeature::place(chunk, random, position.x, position.y, position.z, kind);
    };
}

bool treeDecoration::survivesPlacementPredicate(const Chunk& chunk, const DecorationPos& position,
                                                const std::string_view sapling) {
    if (sapling.empty()) {
        return true;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return false;
    }
    const registry::BlockState* found = registry::blocks::named(sapling);
    if (found == nullptr) {
        throw std::runtime_error{"unknown sapling '" + std::string{sapling} + "'"};
    }
    const auto at = [&](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        return chunk.decorationBlockStateId(position.x + dx, position.y + dy, position.z + dz);
    };
    return registry::blockSupport::canSurvive(found->stateId, at(0, -1, 0), at(0, 1, 0), at(0, 0, -1), at(0, 0, 1),
                                              at(-1, 0, 0), at(1, 0, 0));
}

const std::map<std::string, treeDecoration::Selector, std::less<>>& treeDecoration::selectors() {
    using namespace tree;
    static const std::map<std::string, Selector, std::less<>> built = [] {
        std::map<std::string, Selector, std::less<>> selectors;
        selectors.emplace("trees_plains",
                          Selector{.entries = {{0.33333334F, tree(treeConfigs::fancyOakBees005())},
                                               // Only this entry is `fallen_oak_tree`, which carries its own
                                               // would_survive.
                                               {0.0125F, checkedFallen(fallenTreeConfigs::oak(), "oak_sapling")}},
                                   .defaultCandidate = tree(treeConfigs::oakBees005())});
        selectors.emplace(
            "trees_birch_and_oak_leaf_litter",
            Selector{.entries = {{0.0025F, checkedFallen(fallenTreeConfigs::birch(), "birch_sapling")},
                                 {0.2F, checkedTree(treeConfigs::birchBees0002LeafLitter(), "birch_sapling")},
                                 {0.1F, checkedTree(treeConfigs::fancyOakBees0002LeafLitter(), "oak_sapling")},
                                 {0.0125F, checkedFallen(fallenTreeConfigs::oak(), "oak_sapling")}},
                     .defaultCandidate = checkedTree(treeConfigs::oakBees0002LeafLitter(), "oak_sapling")});
        selectors.emplace("trees_birch",
                          Selector{.entries = {{0.0125F, checkedFallen(fallenTreeConfigs::birch(), "birch_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::birchBees0002(), "birch_sapling")});
        selectors.emplace(
            "birch_tall",
            Selector{.entries = {{0.00625F, checkedFallen(fallenTreeConfigs::superBirch(), "birch_sapling")},
                                 {0.5F, checkedTree(treeConfigs::superBirchBees0002(), "birch_sapling")},
                                 {0.0125F, checkedFallen(fallenTreeConfigs::birch(), "birch_sapling")}},
                     .defaultCandidate = checkedTree(treeConfigs::birchBees0002(), "birch_sapling")});
        selectors.emplace("trees_taiga",
                          Selector{.entries = {{0.33333334F, checkedTree(treeConfigs::pine(), "spruce_sapling")},
                                               {0.0125F, checkedFallen(fallenTreeConfigs::spruce(), "spruce_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::spruce(), "spruce_sapling")});
        selectors.emplace("trees_snowy",
                          Selector{.entries = {{0.0125F, checkedFallen(fallenTreeConfigs::spruce(), "spruce_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::spruce(), "spruce_sapling")});
        selectors.emplace("trees_old_growth_spruce_taiga",
                          Selector{.entries = {{0.33333334F, checkedTree(treeConfigs::megaSpruce(), "spruce_sapling")},
                                               {0.33333334F, checkedTree(treeConfigs::pine(), "spruce_sapling")},
                                               {0.0125F, checkedFallen(fallenTreeConfigs::spruce(), "spruce_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::spruce(), "spruce_sapling")});
        selectors.emplace("trees_old_growth_pine_taiga",
                          Selector{.entries = {{0.025641026F, checkedTree(treeConfigs::megaSpruce(), "spruce_sapling")},
                                               {0.30769232F, checkedTree(treeConfigs::megaPine(), "spruce_sapling")},
                                               {0.33333334F, checkedTree(treeConfigs::pine(), "spruce_sapling")},
                                               {0.0125F, checkedFallen(fallenTreeConfigs::spruce(), "spruce_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::spruce(), "spruce_sapling")});
        const auto savanna = [] {
            return Selector{.entries = {{0.8F, checkedTree(treeConfigs::acacia(), "acacia_sapling")},
                                        {0.0125F, checkedFallen(fallenTreeConfigs::oak(), "oak_sapling")}},
                            .defaultCandidate = checkedTree(treeConfigs::oak(), "oak_sapling")};
        };
        selectors.emplace("trees_savanna", savanna());
        selectors.emplace("trees_windswept_savanna", savanna());
        selectors.emplace("trees_jungle",
                          Selector{.entries = {{0.1F, checkedTree(treeConfigs::fancyOak(), "oak_sapling")},
                                               {0.5F, checkedTree(treeConfigs::jungleBush(), "oak_sapling")},
                                               {0.33333334F, checkedTree(treeConfigs::megaJungle(), "jungle_sapling")},
                                               {0.0125F, checkedFallen(fallenTreeConfigs::jungle(), "jungle_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::jungleTree(), "jungle_sapling")});
        selectors.emplace("trees_sparse_jungle",
                          Selector{.entries = {{0.1F, checkedTree(treeConfigs::fancyOak(), "oak_sapling")},
                                               {0.5F, checkedTree(treeConfigs::jungleBush(), "oak_sapling")},
                                               {0.0125F, checkedFallen(fallenTreeConfigs::jungle(), "jungle_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::jungleTree(), "jungle_sapling")});
        selectors.emplace("trees_windswept_hills", windsweptSelector());
        selectors.emplace("trees_windswept_forest", windsweptSelector());
        selectors.emplace("trees_grove", Selector{.entries = {{0.33333334F, onSnow(treeConfigs::pine())}},
                                                  .defaultCandidate = onSnow(treeConfigs::spruce())});
        selectors.emplace("trees_badlands",
                          Selector{.entries = {{0.0125F, checkedFallen(fallenTreeConfigs::oak(), "oak_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::oakLeafLitter(), "oak_sapling")});
        selectors.emplace("trees_meadow",
                          Selector{.entries = {{0.5F, checkedTree(treeConfigs::fancyOakBees(), "oak_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::superBirchBees(), "birch_sapling")});
        selectors.emplace("trees_water",
                          Selector{.entries = {{0.1F, checkedTree(treeConfigs::fancyOak(), "oak_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::oak(), "oak_sapling")});
        selectors.emplace("trees_flower_forest",
                          Selector{.entries = {{0.0025F, checkedFallen(fallenTreeConfigs::birch(), "birch_sapling")},
                                               {0.2F, checkedTree(treeConfigs::birchBees002(), "birch_sapling")},
                                               {0.1F, checkedTree(treeConfigs::fancyOakBees002(), "oak_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::oakBees002(), "oak_sapling")});
        selectors.emplace("trees_mangrove",
                          Selector{.entries = {{0.85F, checkedTree(treeConfigs::tallMangrove(), "mangrove_propagule")}},
                                   .defaultCandidate = checkedTree(treeConfigs::mangrove(), "mangrove_propagule")});
        // parity: `trees_dappled_forest`, the only weighted tree selector. Every entry carries its own
        // would_survive filter, checked after the pick.
        selectors.emplace(
            "trees_dappled_forest",
            Selector{.weighted = {{200, checkedTree(treeConfigs::redPoplarLeafLitter(), "poplar_sapling")},
                                  {240, checkedTree(treeConfigs::orangePoplarLeafLitter(), "poplar_sapling")},
                                  {90, checkedTree(treeConfigs::yellowPoplarLeafLitter(), "poplar_sapling")},
                                  {27, checkedTree(treeConfigs::spruce(), "spruce_sapling")},
                                  {120, checkedFallen(fallenTreeConfigs::poplar(), "poplar_sapling")}}});
        selectors.emplace("trees_cherry", Selector{.defaultCandidate = tree(treeConfigs::cherryBees005())});
        selectors.emplace("trees_swamp", Selector{.defaultCandidate = tree(treeConfigs::swampOak())});
        selectors.emplace("pale_garden_vegetation",
                          Selector{.entries = {{0.1F, checkedTree(treeConfigs::paleOakCreaking(), "pale_oak_sapling")},
                                               {0.9F, checkedTree(treeConfigs::paleOak(), "pale_oak_sapling")}},
                                   .defaultCandidate = checkedTree(treeConfigs::paleOak(), "pale_oak_sapling")});
        selectors.emplace(
            "dark_forest_vegetation",
            Selector{.entries = {{0.025F, hugeMushroom(hugeMushroomFeature::Kind::Brown)},
                                 {0.05F, hugeMushroom(hugeMushroomFeature::Kind::Red)},
                                 {0.6666667F, checkedTree(treeConfigs::darkOakLeafLitter(), "dark_oak_sapling")},
                                 {0.0025F, checkedFallen(fallenTreeConfigs::birch(), "birch_sapling")},
                                 {0.2F, checkedTree(treeConfigs::birchLeafLitter(), "birch_sapling")},
                                 {0.0125F, checkedFallen(fallenTreeConfigs::oak(), "oak_sapling")},
                                 {0.1F, checkedTree(treeConfigs::fancyOakLeafLitter(), "oak_sapling")}},
                     .defaultCandidate = checkedTree(treeConfigs::oakLeafLitter(), "oak_sapling")});
        return selectors;
    }();
    return built;
}

const treeDecoration::Selector& treeDecoration::selector(const std::string_view placedFeature) {
    const auto found = selectors().find(placedFeature);
    if (found == selectors().end()) {
        throw std::runtime_error{"unknown tree selector '" + std::string{placedFeature} + "'"};
    }
    return found->second;
}

bool treeDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                             const std::string_view placedFeature, random::RandomSource& mossyCarpetRandom) {
    const auto found = selectors().find(placedFeature);
    if (found == selectors().end()) {
        return false;
    }
    const Candidate* candidate = pick(random, found->second);
    if (candidate != nullptr && *candidate) {
        (*candidate)(chunk, random, origin, mossyCarpetRandom);
    }
    return true;
}

bool treeDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                             const std::string_view placedFeature) {
    return placeAt(chunk, random, origin, placedFeature, random);
}

BiomeVegetalDecoration treeDecoration::candidateSequenceCountExtra(std::vector<std::pair<float, Candidate>> entries,
                                                                   Candidate defaultCandidate, const std::int32_t count,
                                                                   const float chance, const std::int32_t extra) {
    return [entries = std::move(entries), defaultCandidate = std::move(defaultCandidate), count, chance,
            extra](Chunk& chunk, random::RandomSource& random) {
        place(chunk, random, countExtraPlacement(count, chance, extra),
              [&](random::RandomSource& r) { return pickSequence(r, entries, defaultCandidate); });
    };
}

BiomeVegetalDecoration treeDecoration::countExtra(const Selector& selector, const std::int32_t count,
                                                  const float chance, const std::int32_t extra) {
    return candidateSequenceCountExtra(selector.entries, selector.defaultCandidate, count, chance, extra);
}

BiomeVegetalDecoration treeDecoration::sequenceOf(const Selector& selector, const std::int32_t frequency) {
    return [&selector, frequency](Chunk& chunk, random::RandomSource& random) {
        place(chunk, random, countPlacement(frequency), [&](random::RandomSource& r) { return pick(r, selector); });
    };
}

BiomeVegetalDecoration treeDecoration::single(const tree::TreeConfiguration& config, const std::int32_t count,
                                              const float chance, const std::int32_t extra) {
    return [candidate = tree(config), count, chance, extra](Chunk& chunk, random::RandomSource& random) {
        place(chunk, random, countExtraPlacement(count, chance, extra),
              [&candidate](random::RandomSource&) { return &candidate; });
    };
}

BiomeVegetalDecoration treeDecoration::fixed(const tree::TreeConfiguration& config, const std::int32_t count) {
    return [candidate = tree(config), count](Chunk& chunk, random::RandomSource& random) {
        place(chunk, random, countPlacement(count), [&candidate](random::RandomSource&) { return &candidate; });
    };
}

BiomeVegetalDecoration treeDecoration::rarityChoice(const tree::TreeConfiguration& rareConfig, const float rareChance,
                                                    const tree::TreeConfiguration& defaultConfig,
                                                    const std::int32_t rarity) {
    return [rare = tree(rareConfig), fallback = tree(defaultConfig), rareChance, rarity](Chunk& chunk,
                                                                                         random::RandomSource& random) {
        place(chunk, random, rarityFilter(rarity),
              [&](random::RandomSource& r) { return r.nextFloat() < rareChance ? &rare : &fallback; });
    };
}

BiomeVegetalDecoration treeDecoration::providerCountChoice(const tree::TreeConfiguration& rareConfig,
                                                           const float rareChance,
                                                           const tree::TreeConfiguration& defaultConfig,
                                                           IntProvider count) {
    return [rare = tree(rareConfig), fallback = tree(defaultConfig), rareChance,
            count = std::move(count)](Chunk& chunk, random::RandomSource& random) {
        place(chunk, random, countPlacement(count),
              [&](random::RandomSource& r) { return r.nextFloat() < rareChance ? &rare : &fallback; });
    };
}

BiomeVegetalDecoration treeDecoration::water() {
    return providerCountChoice(tree::treeConfigs::fancyOak(), 0.1F, tree::treeConfigs::oak(),
                               IntProvider::weighted({{0, 9}, {1, 1}}));
}

} // namespace basalt::server::world::terrain::decoration
