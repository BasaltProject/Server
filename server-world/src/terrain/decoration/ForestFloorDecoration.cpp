#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/ForestFloorDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

// The horizontal provider order is NORTH, EAST, SOUTH, WEST, while block state ids group facings as
// NORTH, SOUTH, WEST, EAST.
constexpr std::array<std::int32_t, 4> kHorizontalFacingStateOffsets{0, 12, 4, 8};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t leafLitterStateId(std::int32_t choice, std::int32_t maxSegmentAmount) {
    const std::int32_t segmentOffset = choice / 4;
    if (segmentOffset >= maxSegmentAmount) {
        throw std::logic_error{"leaf litter segment out of range"};
    }
    return stateIdOf("leaf_litter") + kHorizontalFacingStateOffsets[static_cast<std::size_t>(choice % 4)] +
           segmentOffset;
}

[[nodiscard]] SimpleBlockFeature wildflowerFeature() {
    return SimpleBlockFeature{[](random::RandomSource& random, const DecorationPos&) {
        return forestFloorDecoration::flowerBedStateId(stateIdOf("wildflowers"), random.nextInt(16));
    }};
}

/// parity: the leaf litter patch's own `block_predicate_filter`, which wants grass block below.
[[nodiscard]] PlacementModifier blockBelowFilter(std::int32_t stateId) {
    return filter([stateId](const PlacementContext& context, random::RandomSource&, const DecorationPos& origin) {
        return context.chunk().blockStateId(origin.x & 15, origin.y - 1, origin.z & 15) == stateId;
    });
}

[[nodiscard]] BiomeVegetalDecoration placed(std::vector<PlacementModifier> modifiers, SimpleBlockFeature feature) {
    return
        [modifiers = std::move(modifiers), feature = std::move(feature)](Chunk& chunk, random::RandomSource& random) {
            const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
            placeFrom(chunk, random, origin, modifiers,
                      [&](const DecorationPos& position) { feature.place(chunk, random, position); });
        };
}

} // namespace

std::int32_t forestFloorDecoration::flowerBedStateId(const std::int32_t defaultStateId, const std::int32_t choice) {
    return defaultStateId + kHorizontalFacingStateOffsets[static_cast<std::size_t>(choice % 4)] + choice / 4;
}

const BiomeVegetalDecoration& forestFloorDecoration::birchWildflowers() {
    static const BiomeVegetalDecoration decoration =
        placed({countPlacement(3), rarityFilter(2), inSquarePlacement(), heightmapPlacement(), countPlacement(64),
                randomOffsetPlacement(6, 2), airTargetFilter()},
               wildflowerFeature());
    return decoration;
}

const BiomeVegetalDecoration& forestFloorDecoration::meadowWildflowers() {
    static const BiomeVegetalDecoration decoration =
        placed({noiseThresholdCountPlacement(-0.8, 5, 10), inSquarePlacement(), heightmapPlacement(), countPlacement(8),
                randomOffsetPlacement(6, 2), airTargetFilter()},
               wildflowerFeature());
    return decoration;
}

const BiomeVegetalDecoration& forestFloorDecoration::leafLitter() {
    static const BiomeVegetalDecoration decoration = placed(
        {countPlacement(2), inSquarePlacement(), heightmapPlacement(HeightmapType::WorldSurface), countPlacement(32),
         randomOffsetPlacement(7, 3), airTargetFilter(), blockBelowFilter(stateIdOf("grass_block"))},
        SimpleBlockFeature{[](random::RandomSource& random, const DecorationPos&) {
            return leafLitterStateId(random.nextInt(12), 3);
        }});
    return decoration;
}

void forestFloorDecoration::placeWildflowersAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                               const std::int32_t count) {
    const SimpleBlockFeature feature = wildflowerFeature();
    const std::vector<PlacementModifier> modifiers{countPlacement(count), randomOffsetPlacement(6, 2),
                                                   airTargetFilter()};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { feature.place(chunk, random, position); });
}

} // namespace basalt::server::world::terrain::decoration
