package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** One biome's real `vegetal_decoration` (and friends) generation step. // parity: the `Decoration.VEGETAL_DECORATION`-and-later portion of a biome's own `BiomeGenerationSettings`. */
public fun interface BiomeVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    )
}

/** Runs each of [steps] in order against the same [chunk]/[random]: matches how a real biome's own `BiomeGenerationSettings` runs its `VEGETAL_DECORATION` features one after another over a single shared random stream. */
public fun combine(vararg steps: BiomeVegetalDecoration): BiomeVegetalDecoration =
    BiomeVegetalDecoration { chunk, random -> steps.forEach { it.decorate(chunk, random) } }

/**
 * Dispatches a biome identifier to its decoration, as `ChunkGenerator.applyBiomeDecoration` does.
 * [TreePatchDecoration] and [FlowerPatchDecoration] own the independently seeded tree and flower
 * features and filter each randomized origin by biome. This table retains the smaller legacy
 * tails that have not yet moved to global dispatchers; those tails are still selected from the
 * chunk's centre biome.
 *
 * All 55 Overworld biomes are listed; an unknown one resolves to `null` rather than borrowing
 * another biome's features.
 */
public object BiomeDecoration {
    private val noVegetalDecoration = BiomeVegetalDecoration { _, _ -> }

    private val byBiome: Map<Identifier, BiomeVegetalDecoration> =
        mapOf(
            // parity: TREES_PLAINS, fancy oak 33.3%, fallen oak 1.25%, oak default.
            Identifier.vanilla("plains") to noVegetalDecoration,
            Identifier.vanilla("sunflower_plains") to noVegetalDecoration,
            // parity: TREES_BIRCH_AND_OAK_LEAF_LITTER.
            Identifier.vanilla("forest") to noVegetalDecoration,
            // parity: OverworldBiomes.forest(flower=true): trees_flower_forest, flower_flower_forest,
            // and default grass. Fallen birch and tree decorators remain shared gaps.
            Identifier.vanilla("flower_forest") to noVegetalDecoration,
            // parity: OverworldBiomes.forest(birch=true, tall=false)/addBirchTrees: TREES_BIRCH, birch default.
            Identifier.vanilla("birch_forest") to noVegetalDecoration,
            // parity: OverworldBiomes.forest(birch=true, tall=true)/addTallBirchTrees: BIRCH_TALL, super_birch default.
            Identifier.vanilla("old_growth_birch_forest") to noVegetalDecoration,
            // parity: OverworldBiomes.taiga/addTaigaTrees+addDefaultFlowers: TREES_TAIGA (33.3% pine else spruce,
            // fallen_spruce 1.25%) plus the real default flower patch. Shared ground cover and the
            // taiga mushroom tail are appended by forBiome.
            Identifier.vanilla("taiga") to noVegetalDecoration,
            // parity: snowy taiga uses TREES_TAIGA too, followed by default flowers.
            Identifier.vanilla("snowy_taiga") to noVegetalDecoration,
            // parity: OverworldBiomes.oldGrowthTaiga(spruce=true)/TREES_OLD_GROWTH_SPRUCE_TAIGA: 33.3% mega_spruce,
            // 33.3% pine, fallen_spruce 1.25%, spruce default. countExtra(10, 0.1, 1).
            Identifier.vanilla("old_growth_spruce_taiga") to noVegetalDecoration,
            // parity: OverworldBiomes.oldGrowthTaiga(spruce=false)/TREES_OLD_GROWTH_PINE_TAIGA: 2.56% mega_spruce,
            // 30.8% mega_pine, 33.3% pine, fallen_spruce 1.25%, spruce default.
            Identifier.vanilla("old_growth_pine_taiga") to noVegetalDecoration,
            // parity: OverworldBiomes.plains(snowy=true, spikes=false/true)/addSnowyTrees: same TREES_SNOWY feature.
            Identifier.vanilla("snowy_plains") to noVegetalDecoration,
            Identifier.vanilla("ice_spikes") to noVegetalDecoration,
            // parity: OverworldBiomes.savanna/addSavannaTrees+addWarmFlowers: TREES_SAVANNA (80% acacia else oak,
            // fallen_oak 1.25%) plus flower_warm (real flowerDefault, rarity 1/16).
            Identifier.vanilla("savanna") to noVegetalDecoration,
            // parity: OverworldBiomes.savanna(plateau=true)/addSavannaTrees+addWarmFlowers: same as plains savanna.
            Identifier.vanilla("savanna_plateau") to noVegetalDecoration,
            // parity: OverworldBiomes.savanna(shattered=true)/addShatteredSavannaTrees+addDefaultFlowers (not
            // addWarmFlowers: the shattered variant uses the real default rarity, not the warm one).
            Identifier.vanilla("windswept_savanna") to noVegetalDecoration,
            // parity: TREES_JUNGLE; all selector shapes are present, decorators remain shared gaps.
            Identifier.vanilla("jungle") to noVegetalDecoration,
            // parity: TREES_SPARSE_JUNGLE; all selector shapes are present, decorators remain shared gaps.
            Identifier.vanilla("sparse_jungle") to noVegetalDecoration,
            // parity: OverworldBiomes.windsweptHills(moreTrees=false)/addMountainTrees+addDefaultFlowers+addDefaultGrass:
            // TREES_WINDSWEPT_HILLS: fallen spruce 0.8325%, spruce 66.6%, fancy oak 10%, fallen oak 1.25%, oak default.
            Identifier.vanilla("windswept_hills") to noVegetalDecoration,
            Identifier.vanilla("windswept_gravelly_hills") to noVegetalDecoration,
            // parity: OverworldBiomes.windsweptHills(moreTrees=true)/addMountainForestTrees: TREES_WINDSWEPT_FOREST, same species mix.
            Identifier.vanilla("windswept_forest") to noVegetalDecoration,
            // parity: OverworldBiomes.grove/addGroveTrees: TREES_GROVE, 33.3% pine else spruce (real "on snow" placement filter dropped).
            Identifier.vanilla("grove") to noVegetalDecoration,
            // parity: peaks have no vegetal features; snowy slopes adds only patch_pumpkin later.
            // Explicit empty bases keep their earlier shared disk stage reachable through forBiome.
            Identifier.vanilla("snowy_slopes") to noVegetalDecoration,
            Identifier.vanilla("frozen_peaks") to noVegetalDecoration,
            Identifier.vanilla("jagged_peaks") to noVegetalDecoration,
            Identifier.vanilla("stony_peaks") to noVegetalDecoration,
            // parity: OverworldBiomes.badlands(wooded=true)/addBadlandsTrees+addBadlandGrass: TREES_BADLANDS
            // (oak_leaf_litter default, fallen_oak candidate) plus patch_grass_badlands (now seeded
            // independently by GrassPatchDecoration); dry grass and dead bushes are seeded by
            // GroundPlantPatchDecoration.
            Identifier.vanilla("wooded_badlands") to
                noVegetalDecoration,
            // parity: OverworldBiomes.badlands(wooded=false)/addBadlandGrass; the ground features are
            // independently seeded by GrassPatchDecoration and GroundPlantPatchDecoration.
            Identifier.vanilla("badlands") to noVegetalDecoration,
            Identifier.vanilla("eroded_badlands") to noVegetalDecoration,
            // parity: OverworldBiomes.swamp/addSwampVegetation: TREES_SWAMP (swamp_oak, a single feature, not a
            // random selector) plus flower_swamp/patch_grass_normal/patch_dead_bush/patch_waterlily.
            Identifier.vanilla("swamp") to noVegetalDecoration,
            // parity: dry grass and dead bushes are independently seeded by GroundPlantPatchDecoration;
            // cactus remains in ExtraVegetationDecoration.DESERT.
            Identifier.vanilla("desert") to noVegetalDecoration,
            // parity: OverworldBiomes.darkForest(isPaleGarden=false)/DARK_FOREST_VEGETATION: real RANDOM_SELECTOR,
            // huge brown 2.5%, huge red 5%, dark_oak 66.7%, fallen_birch 0.25%, birch 20%, fallen_oak
            // 1.25%, fancy_oak 10%, oak default. CountPlacement(16), not countExtra.
            Identifier.vanilla("dark_forest") to
                noVegetalDecoration,
            // parity: PALE_GARDEN_VEGETATION rolls creaking 10%, ordinary pale oak 90%, then ordinary fallback.
            // CountPlacement(16). Pale moss remains on the legacy path in forBiome.
            Identifier.vanilla("pale_garden") to
                noVegetalDecoration,
            // parity: meadow tall grass, grass, DualNoiseProvider flowers, rare trees, and wildflowers.
            Identifier.vanilla("meadow") to noVegetalDecoration,
            // parity: OverworldBiomes.meadowOrCherryGrove(cherryGrove=true)/addCherryGroveVegetation:
            // patch_grass_plain + flower_cherry (pink petals) + trees_cherry (countExtra(10, 0.1, 1),
            // cherry_bees_005). See CherryGroveDecoration's own KDoc.
            Identifier.vanilla("cherry_grove") to noVegetalDecoration,
            // parity: river vegetation is composed in forBiome so water trees precede bushes and defaults.
            Identifier.vanilla("river") to noVegetalDecoration,
            Identifier.vanilla("frozen_river") to noVegetalDecoration,
            // parity: OverworldBiomes.beach(snowy=false, stony=false)/addDefaultFlowers+addDefaultGrass.
            Identifier.vanilla("beach") to noVegetalDecoration,
            Identifier.vanilla("snowy_beach") to noVegetalDecoration,
            Identifier.vanilla("stony_shore") to noVegetalDecoration,
            // parity: OverworldBiomes.bambooJungle/baseJungle: addWarmFlowers+addJungleGrass (shared with
            // jungle/sparse_jungle), after dense bamboo and the bamboo_vegetation selector.
            Identifier.vanilla("bamboo_jungle") to noVegetalDecoration,
            // parity: OverworldBiomes.dripstoneCaves/deepDark both call addPlainGrass and addPlainVegetation.
            // GrassPatchDecoration and FlowerPatchDecoration own those exact features independently; using the
            // legacy PlainsDecoration here would also add patch_bush, which these cave biomes do not contain.
            // Real vanilla additionally runs cave-specific dripstone/sculk systems elsewhere in the step.
            Identifier.vanilla("dripstone_caves") to noVegetalDecoration,
            Identifier.vanilla("deep_dark") to noVegetalDecoration,
            // parity: OverworldBiomes.lushCaves/sulfurCaves: both call only addPlainGrass (real patch_tall_grass_2),
            // not the rest of Plains' own decoration. addLushCavesVegetationFeatures/addSulfurCavesFeatures (cave
            // ceiling vegetation, glow berries, azalea, sulfur spikes) are real cave-specific systems, not ported;
            // like dripstone_caves/deep_dark, neither biome is reachable without cave generation yet either.
            Identifier.vanilla("lush_caves") to noVegetalDecoration,
            Identifier.vanilla("sulfur_caves") to noVegetalDecoration,
            // parity: OverworldBiomes.mangroveSwamp/addMangroveSwampVegetation: TREES_MANGROVE (85% tall_mangrove
            // else mangrove, CountPlacement(25) fixed) plus patch_grass_normal/patch_dead_bush/patch_waterlily
            // (no flower_swamp here). Grass and dead bushes are independently seeded; only waterlilies
            // remain on the approximate shared stream.
            // Real MangroveRootPlacer ported: trunks start elevated on simulated roots.
            Identifier.vanilla("mangrove_swamp") to
                noVegetalDecoration,
            // parity: OverworldBiomes.mushroomFields/addMushroomFieldVegetation: mushroom_island_vegetation
            // (huge red/brown 50/50, one attempt per chunk) + brown_mushroom_taiga (rarity 4) +
            // red_mushroom_taiga (rarity 256). Shared near-water vegetation is appended by forBiome.
            Identifier.vanilla("mushroom_fields") to MushroomFieldsDecoration.HUGE_ONLY,
            // Seagrass, kelp and sea pickles are independently seeded by AquaticPatchDecoration.
            Identifier.vanilla("ocean") to noVegetalDecoration,
            Identifier.vanilla("deep_ocean") to noVegetalDecoration,
            Identifier.vanilla("cold_ocean") to noVegetalDecoration,
            Identifier.vanilla("deep_cold_ocean") to noVegetalDecoration,
            Identifier.vanilla("lukewarm_ocean") to noVegetalDecoration,
            Identifier.vanilla("deep_lukewarm_ocean") to noVegetalDecoration,
            // Coral remains on the legacy path until its own exact global-index port.
            Identifier.vanilla("warm_ocean") to CoralDecoration,
            // parity: frozenOcean adds packed/blue icebergs in LOCAL_MODIFICATIONS, blue ice in
            // SURFACE_STRUCTURES, then water trees and the same default vegetation as beaches.
            Identifier.vanilla("frozen_ocean") to noVegetalDecoration,
            Identifier.vanilla("deep_frozen_ocean") to noVegetalDecoration,
        )

    public fun forBiome(id: Identifier): BiomeVegetalDecoration? {
        val path = id.path
        val body =
            when (path) {
                "plains" -> requireBase(id)
                "sunflower_plains" -> requireBase(id)
                "ocean", "deep_ocean", "cold_ocean", "deep_cold_ocean", "lukewarm_ocean", "deep_lukewarm_ocean" ->
                    oceanWithExtra(noVegetalDecoration)
                "warm_ocean" -> oceanWithExtra(CoralDecoration)
                "frozen_ocean", "deep_frozen_ocean" -> requireBase(id)
                "river" -> requireBase(id)
                "frozen_river" -> requireBase(id)
                "swamp" ->
                    requireBase(id)
                "mangrove_swamp" ->
                    requireBase(id)
                "desert" -> requireBase(id)
                "badlands", "eroded_badlands", "wooded_badlands" -> requireBase(id)
                "mushroom_fields" -> requireBase(id)
                "jungle", "bamboo_jungle" ->
                    requireBase(id)
                "sparse_jungle" ->
                    requireBase(id)
                "savanna", "savanna_plateau" -> requireBase(id)
                "windswept_savanna" -> requireBase(id)
                "forest" -> requireBase(id)
                "birch_forest", "old_growth_birch_forest" -> requireBase(id)
                "dark_forest" -> requireBase(id)
                "pale_garden" -> requireBase(id)
                "flower_forest" -> requireBase(id)
                "windswept_hills", "windswept_gravelly_hills", "windswept_forest" -> requireBase(id)
                in OLD_GROWTH_TAIGA_PATHS -> requireBase(id)
                "taiga" -> requireBase(id)
                "snowy_taiga" -> requireBase(id)
                in DEFAULT_EXTRA_PATHS -> requireBase(id)
                in PUMPKIN_ONLY_PATHS ->
                    requireBase(id)
                else -> byBiome[id]
            }
        return body
    }

    private fun oceanWithExtra(aquatic: BiomeVegetalDecoration): BiomeVegetalDecoration = aquatic

    private fun requireBase(id: Identifier): BiomeVegetalDecoration =
        byBiome[id] ?: error("missing base decoration for $id")

    private val DEFAULT_EXTRA_PATHS: Set<String> =
        setOf(
            "snowy_plains",
            "ice_spikes",
            "beach",
            "snowy_beach",
            "stony_shore",
            "frozen_ocean",
            "deep_frozen_ocean",
        )

    private val PUMPKIN_ONLY_PATHS: Set<String> =
        setOf("grove", "snowy_slopes", "dripstone_caves", "deep_dark")

    private val OLD_GROWTH_TAIGA_PATHS: Set<String> = setOf("old_growth_pine_taiga", "old_growth_spruce_taiga")
}
