#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>
#include <basalt/server/world/terrain/decoration/TaigaGroundDecoration.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

// The 26.2 state definition orders age=0..3 contiguously. The generated default-state table
// identifies age=0; the configured berry_bush feature explicitly requests age=3.
[[nodiscard]] std::int32_t sweetBerryAge3StateId() { return stateIdOf("sweet_berry_bush") + 3; }

[[nodiscard]] PlacementModifier grassBelowFilter() {
    return filter([](const PlacementContext& context, random::RandomSource&, const DecorationPos& origin) {
        return context.chunk().blockStateId(origin.x & 15, origin.y - 1, origin.z & 15) ==
               registry::blocks::named("grass_block")->stateId;
    });
}

[[nodiscard]] BiomeVegetalDecoration placed(std::vector<PlacementModifier> modifiers, SimpleBlockFeature feature) {
    return [modifiers = std::move(modifiers), feature = std::move(feature)](
               Chunk& chunk, random::RandomSource& random) { runFromChunkCorner(chunk, random, modifiers, feature); };
}

[[nodiscard]] BiomeVegetalDecoration grass(std::int32_t outerCount) {
    std::vector<PlacementModifier> modifiers;
    if (outerCount != 1) {
        modifiers.push_back(countPlacement(outerCount));
    }
    modifiers.push_back(inSquarePlacement());
    modifiers.push_back(heightmapPlacement(HeightmapType::WorldSurface));
    modifiers.push_back(countPlacement(32));
    modifiers.push_back(randomOffsetPlacement(7, 3));
    modifiers.push_back(airTargetFilter());
    return placed(std::move(modifiers),
                  SimpleBlockFeature{[](random::RandomSource& providerRandom, const DecorationPos&) {
                      return providerRandom.nextInt(5) == 0 ? stateIdOf("short_grass") : stateIdOf("fern");
                  }});
}

[[nodiscard]] BiomeVegetalDecoration berry(std::int32_t chance) {
    return placed(
        {rarityFilter(chance), inSquarePlacement(), heightmapPlacement(HeightmapType::WorldSurface), countPlacement(96),
         randomOffsetPlacement(7, 3), airTargetFilter(), grassBelowFilter()},
        SimpleBlockFeature{[](random::RandomSource&, const DecorationPos&) { return sweetBerryAge3StateId(); }});
}

} // namespace

BiomeVegetalDecoration taigaGroundDecoration::rareBerry() { return berry(384); }

BiomeVegetalDecoration taigaGroundDecoration::commonBerry() { return berry(32); }

BiomeVegetalDecoration taigaGroundDecoration::largeFern() {
    return placed(
        {rarityFilter(5), inSquarePlacement(), heightmapPlacement(), countPlacement(96), randomOffsetPlacement(7, 3),
         airTargetFilter(true)},
        SimpleBlockFeature{[](random::RandomSource&, const DecorationPos&) { return stateIdOf("large_fern"); }, true});
}

BiomeVegetalDecoration taigaGroundDecoration::normal() { return grass(1); }

BiomeVegetalDecoration taigaGroundDecoration::oldGrowthDeadBush() {
    return placed(
        {inSquarePlacement(), heightmapPlacement(HeightmapType::WorldSurface), countPlacement(4),
         randomOffsetPlacement(7, 3), airTargetFilter()},
        SimpleBlockFeature{[](random::RandomSource&, const DecorationPos&) { return stateIdOf("dead_bush"); }});
}

BiomeVegetalDecoration taigaGroundDecoration::oldGrowth() { return combine({grass(7), oldGrowthDeadBush()}); }

} // namespace basalt::server::world::terrain::decoration
