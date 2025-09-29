#include <basalt/server/world/terrain/decoration/BambooVegetationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>

#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

void placeCheckedTree(Chunk& chunk, random::RandomSource& random, const DecorationPos& position,
                      const tree::TreeConfiguration& config, std::string_view sapling) {
    if (treeDecoration::survivesPlacementPredicate(chunk, position, sapling)) {
        tree::treeFeature::place(chunk, random, position.x, position.y, position.z, config);
    }
}

} // namespace

void bambooVegetationDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& position,
                                         const PlacementContext&) {
    if (random.nextFloat() < 0.05F) {
        placeCheckedTree(chunk, random, position, tree::treeConfigs::fancyOak(), "oak_sapling");
        return;
    }
    if (random.nextFloat() < 0.15F) {
        placeCheckedTree(chunk, random, position, tree::treeConfigs::jungleBush(), "oak_sapling");
        return;
    }
    if (random.nextFloat() < 0.7F) {
        placeCheckedTree(chunk, random, position, tree::treeConfigs::megaJungle(), "jungle_sapling");
        return;
    }
    jungleGrassDecoration::placePatchAt(chunk, random, position);
}

void bambooVegetationDecoration::decorate(Chunk& chunk, random::RandomSource& random) {
    const PlacementContext context{chunk};
    const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    const std::vector<PlacementModifier> modifiers{countPlacement(IntProvider::weighted({{30, 9}, {31, 1}})),
                                                   inSquarePlacement(), surfaceWaterDepthFilter(0),
                                                   heightmapPlacement(HeightmapType::OceanFloor)};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { placeAt(chunk, random, position, context); });
}

} // namespace basalt::server::world::terrain::decoration
