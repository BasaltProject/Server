#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>

namespace basalt::server::world::terrain::jigsawStructures {

const JigsawStructureSettings& pillagerOutpost() {
    static const JigsawStructureSettings built{
        .name = "pillager_outpost",
        // Position among the 44 structures of `surface_structures`, in registry order.
        .structureIndex = 27,
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
        .structureIndex = 4,
        .step = 3,
        .startPool = "minecraft:trial_chambers/chamber/end",
        .maxDepth = 20,
        .maxDistanceFromCenter = 116,
        // parity: the chamber is the only jigsaw structure with `pool_aliases`.
        .poolAliases =
            {
                {.kind = JigsawStructureSettings::PoolAlias::Kind::RandomGroup,
                 .groups =
                     {
                         {{"minecraft:trial_chambers/spawner/contents/ranged",
                           "minecraft:trial_chambers/spawner/ranged/skeleton"},
                          {"minecraft:trial_chambers/spawner/contents/slow_ranged",
                           "minecraft:trial_chambers/spawner/slow_ranged/skeleton"}},
                         {{"minecraft:trial_chambers/spawner/contents/ranged",
                           "minecraft:trial_chambers/spawner/ranged/stray"},
                          {"minecraft:trial_chambers/spawner/contents/slow_ranged",
                           "minecraft:trial_chambers/spawner/slow_ranged/stray"}},
                         {{"minecraft:trial_chambers/spawner/contents/ranged",
                           "minecraft:trial_chambers/spawner/ranged/poison_skeleton"},
                          {"minecraft:trial_chambers/spawner/contents/slow_ranged",
                           "minecraft:trial_chambers/spawner/slow_ranged/poison_skeleton"}},
                     }},
                {.kind = JigsawStructureSettings::PoolAlias::Kind::Random,
                 .alias = "minecraft:trial_chambers/spawner/contents/melee",
                 .targets = {"minecraft:trial_chambers/spawner/melee/zombie",
                             "minecraft:trial_chambers/spawner/melee/husk",
                             "minecraft:trial_chambers/spawner/melee/spider"}},
                {.kind = JigsawStructureSettings::PoolAlias::Kind::Random,
                 .alias = "minecraft:trial_chambers/spawner/contents/small_melee",
                 .targets = {"minecraft:trial_chambers/spawner/small_melee/slime",
                             "minecraft:trial_chambers/spawner/small_melee/cave_spider",
                             "minecraft:trial_chambers/spawner/small_melee/silverfish",
                             "minecraft:trial_chambers/spawner/small_melee/baby_zombie"}},
            },
        .dimensionPadding = 10,
        .heightSource = JigsawHeightSource::Drawn,
        .startY = -40,
        .startYMax = -20,
        // `encapsulate` adds density rather than carving.
        .adjustment = TerrainAdjustment::Encapsulate,
        .startRecords = JigsawStartRecords::FromFirstPiece,
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
