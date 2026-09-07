#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

[[nodiscard]] std::int32_t state(std::string_view name) {
    const registry::BlockState* found = registry::blocks::named(name);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{name} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] TreeConfiguration withDecorators(TreeConfiguration base,
                                               std::vector<TreeDecoratorConfiguration> decorators) {
    base.decorators = std::move(decorators);
    return base;
}

/// parity: `sparseLeafLitter` followed by `thickLeafLitter` on the dedicated 26.2 tree configs.
[[nodiscard]] std::vector<TreeDecoratorConfiguration> leafLitterDecorators() {
    return {decorator::GroundLeafLitter{.tries = 96, .radius = 4, .height = 2, .maxSegmentAmount = 3},
            decorator::GroundLeafLitter{.tries = 150, .radius = 2, .height = 2, .maxSegmentAmount = 4}};
}

[[nodiscard]] std::vector<TreeDecoratorConfiguration> withBees(float probability) {
    std::vector<TreeDecoratorConfiguration> decorators{decorator::Beehive{.probability = probability}};
    for (TreeDecoratorConfiguration& litter : leafLitterDecorators()) {
        decorators.push_back(std::move(litter));
    }
    return decorators;
}

} // namespace

const TreeConfiguration& treeConfigs::oak() {
    // parity: TreeFeatures.createOak, createStraightBlobTree(OAK_LOG, OAK_LEAVES, 4, 2, 0, 2).
    static const TreeConfiguration config{.trunkStateId = state("oak_log"),
                                          .trunkPlacer = TrunkPlacer::straight(4, 2, 0),
                                          .foliageStateId = state("oak_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::blob(IntProvider::of(2), IntProvider::of(0), 3),
                                          .minimumSize = FeatureSize::twoLayers(1, 0, 1),
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::azalea() {
    // parity: configured_feature/azalea_tree.
    static const TreeConfiguration config{
        .trunkStateId = state("oak_log"),
        .trunkPlacer = TrunkPlacer::bending(4, 2, 0, 3, IntProvider::uniform(1, 2), state("rooted_dirt")),
        .foliageStateId = state("azalea_leaves"),
        .foliagePlacer =
            FoliagePlacer::weightedRandomSpread(IntProvider::of(3), IntProvider::of(0), IntProvider::of(2), 50,
                                                {{state("azalea_leaves"), 3}, {state("flowering_azalea_leaves"), 1}}),
        .minimumSize = FeatureSize::twoLayers(1, 0, 1)};
    return config;
}

const TreeConfiguration& treeConfigs::swampOak() {
    // parity: TreeFeatures.createStraightBlobTree(OAK_LOG, OAK_LEAVES, 5, 3, 0, 3).
    static const TreeConfiguration config{.trunkStateId = state("oak_log"),
                                          .trunkPlacer = TrunkPlacer::straight(5, 3, 0),
                                          .foliageStateId = state("oak_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::blob(IntProvider::of(3), IntProvider::of(0), 3),
                                          .minimumSize = FeatureSize::twoLayers(1, 0, 1),
                                          .decorators = {decorator::LeafVines{.probability = 0.25F}}};
    return config;
}

const TreeConfiguration& treeConfigs::birch() {
    // parity: TreeFeatures.createBirch, createStraightBlobTree(BIRCH_LOG, BIRCH_LEAVES, 5, 2, 0, 2).
    static const TreeConfiguration config{.trunkStateId = state("birch_log"),
                                          .trunkPlacer = TrunkPlacer::straight(5, 2, 0),
                                          .foliageStateId = state("birch_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::blob(IntProvider::of(2), IntProvider::of(0), 3),
                                          .minimumSize = FeatureSize::twoLayers(1, 0, 1),
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::superBirch() {
    // parity: TreeFeatures.createSuperBirch, createStraightBlobTree(BIRCH_LOG, BIRCH_LEAVES, 5, 2, 6, 2).
    static const TreeConfiguration config{.trunkStateId = state("birch_log"),
                                          .trunkPlacer = TrunkPlacer::straight(5, 2, 6),
                                          .foliageStateId = state("birch_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::blob(IntProvider::of(2), IntProvider::of(0), 3),
                                          .minimumSize = FeatureSize::twoLayers(1, 0, 1),
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::fancyOak() {
    // parity: TreeFeatures.FANCY_OAK registration.
    static const TreeConfiguration config{.trunkStateId = state("oak_log"),
                                          .trunkPlacer = TrunkPlacer::fancy(3, 11, 0),
                                          .foliageStateId = state("oak_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::fancy(IntProvider::of(2), IntProvider::of(4), 4),
                                          .minimumSize = FeatureSize::twoLayers(0, 0, 0, 4),
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::jungleTree() {
    // parity: TreeFeatures.createJungleTree, createStraightBlobTree(JUNGLE_LOG, JUNGLE_LEAVES, 4, 8, 0, 2).
    static const TreeConfiguration config{.trunkStateId = state("jungle_log"),
                                          .trunkPlacer = TrunkPlacer::straight(4, 8, 0),
                                          .foliageStateId = state("jungle_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::blob(IntProvider::of(2), IntProvider::of(0), 3),
                                          .minimumSize = FeatureSize::twoLayers(1, 0, 1),
                                          .decorators = {decorator::Cocoa{.probability = 0.2F}, decorator::TrunkVines{},
                                                         decorator::LeafVines{.probability = 0.25F}},
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::jungleBush() {
    // parity: TreeFeatures.JUNGLE_BUSH registration.
    static const TreeConfiguration config{.trunkStateId = state("jungle_log"),
                                          .trunkPlacer = TrunkPlacer::straight(1, 0, 0),
                                          .foliageStateId = state("oak_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::bush(IntProvider::of(2), IntProvider::of(1), 2),
                                          .minimumSize = FeatureSize::twoLayers(0, 0, 0)};
    return config;
}

const TreeConfiguration& treeConfigs::megaJungle() {
    // parity: TreeFeatures.MEGA_JUNGLE_TREE registration, including trunk and leaf vines.
    static const TreeConfiguration config{
        .trunkStateId = state("jungle_log"),
        .trunkPlacer = TrunkPlacer::megaJungle(10, 2, 19),
        .foliageStateId = state("jungle_leaves"),
        .foliagePlacer = FoliagePlacer::megaJungle(IntProvider::of(2), IntProvider::of(0), 2),
        .minimumSize = FeatureSize::twoLayers(1, 1, 2),
        .decorators = {decorator::TrunkVines{}, decorator::LeafVines{.probability = 0.25F}}};
    return config;
}

const TreeConfiguration& treeConfigs::spruce() {
    // parity: TreeFeatures.SPRUCE registration.
    static const TreeConfiguration config{
        .trunkStateId = state("spruce_log"),
        .trunkPlacer = TrunkPlacer::straight(5, 2, 1),
        .foliageStateId = state("spruce_leaves"),
        .foliagePlacer =
            FoliagePlacer::spruce(IntProvider::uniform(2, 3), IntProvider::uniform(0, 2), IntProvider::uniform(1, 2)),
        .minimumSize = FeatureSize::twoLayers(2, 0, 2),
        .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::pine() {
    // parity: TreeFeatures.PINE registration.
    static const TreeConfiguration config{
        .trunkStateId = state("spruce_log"),
        .trunkPlacer = TrunkPlacer::straight(6, 4, 0),
        .foliageStateId = state("spruce_leaves"),
        .foliagePlacer = FoliagePlacer::pine(IntProvider::of(1), IntProvider::of(1), IntProvider::uniform(3, 4)),
        .minimumSize = FeatureSize::twoLayers(2, 0, 2),
        .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::acacia() {
    // parity: TreeFeatures.ACACIA registration.
    static const TreeConfiguration config{.trunkStateId = state("acacia_log"),
                                          .trunkPlacer = TrunkPlacer::forking(5, 2, 2),
                                          .foliageStateId = state("acacia_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::acacia(IntProvider::of(2), IntProvider::of(0)),
                                          .minimumSize = FeatureSize::twoLayers(1, 0, 2),
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::darkOak() {
    // parity: TreeFeatures.createDarkOak.
    static const TreeConfiguration config{.trunkStateId = state("dark_oak_log"),
                                          .trunkPlacer = TrunkPlacer::darkOak(6, 2, 1),
                                          .foliageStateId = state("dark_oak_leaves"),
                                          .foliagePlacer =
                                              FoliagePlacer::darkOak(IntProvider::of(0), IntProvider::of(0)),
                                          .minimumSize = FeatureSize::threeLayers(1, 1, 0, 1, 2),
                                          .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::paleOak() {
    // parity: TreeFeatures.PALE_OAK, the dark oak shape in pale oak blocks.
    static const TreeConfiguration config{
        .trunkStateId = state("pale_oak_log"),
        .trunkPlacer = TrunkPlacer::darkOak(6, 2, 1),
        .foliageStateId = state("pale_oak_leaves"),
        .foliagePlacer = FoliagePlacer::darkOak(IntProvider::of(0), IntProvider::of(0)),
        .minimumSize = FeatureSize::threeLayers(1, 1, 0, 1, 2),
        .decorators = {decorator::PaleMoss{
            .leavesProbability = 0.15F, .trunkProbability = 0.4F, .groundProbability = 0.8F}},
        .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::paleOakCreaking() {
    // parity: TreeFeatures.PALE_OAK_CREAKING, pale moss then a guaranteed heart attempt.
    static const TreeConfiguration config{
        .trunkStateId = state("pale_oak_log"),
        .trunkPlacer = TrunkPlacer::darkOak(6, 2, 1),
        .foliageStateId = state("pale_oak_leaves"),
        .foliagePlacer = FoliagePlacer::darkOak(IntProvider::of(0), IntProvider::of(0)),
        .minimumSize = FeatureSize::threeLayers(1, 1, 0, 1, 2),
        .decorators = {decorator::PaleMoss{
                           .leavesProbability = 0.15F, .trunkProbability = 0.4F, .groundProbability = 0.8F},
                       decorator::CreakingHeart{.probability = 1.0F}},
        .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::megaSpruce() {
    // parity: TreeFeatures.MEGA_SPRUCE registration.
    static const TreeConfiguration config{
        .trunkStateId = state("spruce_log"),
        .trunkPlacer = TrunkPlacer::giant(13, 2, 14),
        .foliageStateId = state("spruce_leaves"),
        .foliagePlacer = FoliagePlacer::megaPine(IntProvider::of(0), IntProvider::of(0), IntProvider::uniform(13, 17)),
        .minimumSize = FeatureSize::twoLayers(1, 1, 2),
        .decorators = {decorator::AlterGroundToPodzol{}}};
    return config;
}

const TreeConfiguration& treeConfigs::megaPine() {
    // parity: TreeFeatures.MEGA_PINE registration.
    static const TreeConfiguration config{
        .trunkStateId = state("spruce_log"),
        .trunkPlacer = TrunkPlacer::giant(13, 2, 14),
        .foliageStateId = state("spruce_leaves"),
        .foliagePlacer = FoliagePlacer::megaPine(IntProvider::of(0), IntProvider::of(0), IntProvider::uniform(3, 7)),
        .minimumSize = FeatureSize::twoLayers(1, 1, 2),
        .decorators = {decorator::AlterGroundToPodzol{}}};
    return config;
}

const TreeConfiguration& treeConfigs::mangrove() {
    // parity: TreeFeatures.MANGROVE, the small variant.
    static const TreeConfiguration config{
        .trunkStateId = state("mangrove_log"),
        .trunkPlacer =
            TrunkPlacer::upwardsBranching(2, 1, 4, IntProvider::uniform(1, 4), 0.5F, IntProvider::uniform(0, 1)),
        .foliageStateId = state("mangrove_leaves"),
        .foliagePlacer = FoliagePlacer::randomSpread(IntProvider::of(3), IntProvider::of(0), IntProvider::of(2), 70),
        .minimumSize = FeatureSize::twoLayers(2, 0, 2),
        .rootPlacer = MangroveRootPlacer{IntProvider::uniform(1, 3)},
        .decorators = {decorator::LeafVines{.probability = 0.125F},
                       decorator::MangrovePropagules{.probability = 0.14F,
                                                     .exclusionRadiusXZ = 1,
                                                     .exclusionRadiusY = 0,
                                                     .requiredEmptyBlocks = 2},
                       decorator::Beehive{.probability = 0.01F}},
        .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::tallMangrove() {
    // parity: TreeFeatures.TALL_MANGROVE registration.
    static const TreeConfiguration config{
        .trunkStateId = state("mangrove_log"),
        .trunkPlacer =
            TrunkPlacer::upwardsBranching(4, 1, 9, IntProvider::uniform(1, 6), 0.5F, IntProvider::uniform(0, 1)),
        .foliageStateId = state("mangrove_leaves"),
        .foliagePlacer = FoliagePlacer::randomSpread(IntProvider::of(3), IntProvider::of(0), IntProvider::of(2), 70),
        .minimumSize = FeatureSize::twoLayers(3, 0, 2),
        .rootPlacer = MangroveRootPlacer{IntProvider::uniform(3, 7)},
        .decorators = {decorator::LeafVines{.probability = 0.125F},
                       decorator::MangrovePropagules{.probability = 0.14F,
                                                     .exclusionRadiusXZ = 1,
                                                     .exclusionRadiusY = 0,
                                                     .requiredEmptyBlocks = 2},
                       decorator::Beehive{.probability = 0.01F}},
        .ignoreVines = true};
    return config;
}

const TreeConfiguration& treeConfigs::cherry() {
    // parity: TreeFeatures.cherry. branchCount is the real WeightedListInt over ConstantInt(1/2/3),
    // one weight each.
    static const TreeConfiguration config{
        .trunkStateId = state("cherry_log"),
        .trunkPlacer = TrunkPlacer::cherry(7, 1, 0, IntProvider::weighted({{1, 1}, {2, 1}, {3, 1}}),
                                           IntProvider::uniform(2, 4), -4, -3, IntProvider::uniform(-1, 0)),
        .foliageStateId = state("cherry_leaves"),
        .foliagePlacer = FoliagePlacer::cherry(IntProvider::of(4), IntProvider::of(0), IntProvider::of(5), 0.25F, 0.5F,
                                               0.16666667F, 0.33333334F),
        .minimumSize = FeatureSize::twoLayers(1, 0, 2),
        .ignoreVines = true};
    return config;
}

// The same trees again with a different decorator list, which is all these registrations are.
const TreeConfiguration& treeConfigs::oakLeafLitter() {
    static const TreeConfiguration config = withDecorators(oak(), leafLitterDecorators());
    return config;
}

const TreeConfiguration& treeConfigs::birchLeafLitter() {
    static const TreeConfiguration config = withDecorators(birch(), leafLitterDecorators());
    return config;
}

const TreeConfiguration& treeConfigs::fancyOakLeafLitter() {
    static const TreeConfiguration config = withDecorators(fancyOak(), leafLitterDecorators());
    return config;
}

const TreeConfiguration& treeConfigs::darkOakLeafLitter() {
    static const TreeConfiguration config = withDecorators(darkOak(), leafLitterDecorators());
    return config;
}

const TreeConfiguration& treeConfigs::oakBees0002LeafLitter() {
    static const TreeConfiguration config = withDecorators(oak(), withBees(0.002F));
    return config;
}

const TreeConfiguration& treeConfigs::birchBees0002LeafLitter() {
    static const TreeConfiguration config = withDecorators(birch(), withBees(0.002F));
    return config;
}

const TreeConfiguration& treeConfigs::fancyOakBees0002LeafLitter() {
    static const TreeConfiguration config = withDecorators(fancyOak(), withBees(0.002F));
    return config;
}

const TreeConfiguration& treeConfigs::oakBees002() {
    static const TreeConfiguration config = withDecorators(oak(), {decorator::Beehive{.probability = 0.02F}});
    return config;
}

const TreeConfiguration& treeConfigs::birchBees002() {
    static const TreeConfiguration config = withDecorators(birch(), {decorator::Beehive{.probability = 0.02F}});
    return config;
}

const TreeConfiguration& treeConfigs::fancyOakBees002() {
    static const TreeConfiguration config = withDecorators(fancyOak(), {decorator::Beehive{.probability = 0.02F}});
    return config;
}

const TreeConfiguration& treeConfigs::oakBees005() {
    static const TreeConfiguration config = withDecorators(oak(), {decorator::Beehive{.probability = 0.05F}});
    return config;
}

const TreeConfiguration& treeConfigs::fancyOakBees005() {
    static const TreeConfiguration config = withDecorators(fancyOak(), {decorator::Beehive{.probability = 0.05F}});
    return config;
}

const TreeConfiguration& treeConfigs::birchBees0002() {
    static const TreeConfiguration config = withDecorators(birch(), {decorator::Beehive{.probability = 0.002F}});
    return config;
}

const TreeConfiguration& treeConfigs::superBirchBees0002() {
    static const TreeConfiguration config = withDecorators(superBirch(), {decorator::Beehive{.probability = 0.002F}});
    return config;
}

const TreeConfiguration& treeConfigs::fancyOakBees() {
    static const TreeConfiguration config = withDecorators(fancyOak(), {decorator::Beehive{.probability = 1.0F}});
    return config;
}

const TreeConfiguration& treeConfigs::superBirchBees() {
    static const TreeConfiguration config = withDecorators(superBirch(), {decorator::Beehive{.probability = 1.0F}});
    return config;
}

const TreeConfiguration& treeConfigs::cherryBees005() {
    static const TreeConfiguration config = withDecorators(cherry(), {decorator::Beehive{.probability = 0.05F}});
    return config;
}

} // namespace basalt::server::world::terrain::decoration::tree
