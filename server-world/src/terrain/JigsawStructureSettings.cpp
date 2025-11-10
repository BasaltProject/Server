#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>

namespace basalt::server::world::terrain::jigsawStructures {

const JigsawStructureSettings& pillagerOutpost() {
    static const JigsawStructureSettings built{
        .name = "pillager_outpost",
        // Tenth of the 26 structures of `surface_structures`, in registry order.
        .structureIndex = 9,
        .step = 4,
        .startPool = "minecraft:pillager_outpost/base_plates",
        .maxDepth = 7,
        .maxDistanceFromCenter = 80,
        .useExpansionHack = true,
        .projectStartToHeightmap = true,
        .heightSource = JigsawHeightSource::Surface,
        // parity: `start_height: {absolute: 0}`, which draws nothing from the random.
        .startY = 0,
        .adjustment = TerrainAdjustment::BeardThin,
        .startRecords = JigsawStartRecords::FromCandidate,
    };
    return built;
}

const JigsawStructureSettings& trailRuins() {
    static const JigsawStructureSettings built{
        .name = "trail_ruins",
        // Fourth of the five structures of `underground_structures`, in registry order.
        .structureIndex = 3,
        .step = 3,
        .startPool = "minecraft:trail_ruins/tower",
        .maxDepth = 7,
        .maxDistanceFromCenter = 80,
        .projectStartToHeightmap = true,
        .heightSource = JigsawHeightSource::Surface,
        // parity: `start_height: {absolute: -15}`, which draws nothing from the random.
        .startY = -15,
        // `bury` fills ground back over the pieces instead of clearing a hollow under them, so only
        // their top course shows.
        .adjustment = TerrainAdjustment::Bury,
        .startRecords = JigsawStartRecords::FromCandidate,
        .passWorldSeed = true,
    };
    return built;
}

const JigsawStructureSettings& ancientCity() {
    static const JigsawStructureSettings built{
        .name = "ancient_city",
        // First of `underground_decoration`'s three, ahead of the fortress and the fossil.
        .structureIndex = 0,
        .step = 7,
        .startPool = "minecraft:ancient_city/city_center",
        .startJigsawName = "minecraft:city_anchor",
        .maxDepth = 7,
        .maxDistanceFromCenter = 116,
        .heightSource = JigsawHeightSource::Fixed,
        .startY = -27,
        .adjustment = TerrainAdjustment::BeardBox,
        .startRecords = JigsawStartRecords::FromFirstPiece,
        .cacheCapacity = 16,
    };
    return built;
}

const JigsawStructureSettings& trialChamber() {
    static const JigsawStructureSettings built{
        .name = "trial_chambers",
        // Last of the five in `underground_structures`, after the treasure, the two mineshafts and
        // the trail ruins.
        .structureIndex = 4,
        .step = 3,
        .startPool = "minecraft:trial_chambers/chamber/end",
        .maxDepth = 20,
        .maxDistanceFromCenter = 116,
        .heightSource = JigsawHeightSource::Drawn,
        .startY = -40,
        .startYMax = -20,
        // `encapsulate` adds density rather than carving.
        .adjustment = TerrainAdjustment::Encapsulate,
        // Gap: the frozen Kotlin records no start for a chamber, so neither does this. It is the one
        // thing here the jar does that both implementations still miss.
        .startRecords = JigsawStartRecords::None,
        .cacheCapacity = 16,
    };
    return built;
}

const structure::StructurePlacementGrid& trailRuinsGrid() {
    static const structure::StructurePlacementGrid built{34, 8, 83469867};
    return built;
}

const structure::StructurePlacementGrid& ancientCityGrid() {
    static const structure::StructurePlacementGrid built{24, 8, 20083232};
    return built;
}

const structure::StructurePlacementGrid& trialChamberGrid() {
    static const structure::StructurePlacementGrid built{34, 12, 94251327};
    return built;
}

} // namespace basalt::server::world::terrain::jigsawStructures
