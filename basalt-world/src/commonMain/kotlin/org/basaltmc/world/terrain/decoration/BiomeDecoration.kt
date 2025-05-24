package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs

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
 * Dispatches a biome identifier to its real decoration, matching real vanilla's own
 * `ChunkGenerator.applyBiomeDecoration` (which looks up the actual biome at each decorated
 * position, not a single fixed one). [org.basaltmc.world.terrain.RealTerrainGenerator] resolves
 * only the chunk's own center column and decorates the whole chunk with that one result: real
 * vanilla's decoration is chunk-scoped in practice too (everything placed by one chunk's
 * `applyBiomeDecoration` call shares that chunk's carved region), and this project's own decoration
 * pipeline is already chunk-scoped ([PlacementContext][org.basaltmc.world.terrain.decoration.PlacementContext]
 * has no cross-chunk state), so per-column dispatch is not meaningfully different except at biome
 * borders, an accepted, documented approximation.
 *
 * All 55 current Overworld biome identifiers are listed. Unknown biome identifiers resolve to
 * `null` rather than borrowing another biome's feature list.
 */
public object BiomeDecoration {
    private val noVegetalDecoration = BiomeVegetalDecoration { _, _ -> }

    private val byBiome: Map<Identifier, BiomeVegetalDecoration> =
        mapOf(
            // parity: OverworldBiomes.plains(sunflower=false)/BiomeDefaultFeatures.addPlainVegetation.
            // TREES_PLAINS: countExtra(0, 0.05, 1), oak default (fancy_oak/fallen_oak entries dropped, see TreeDecoration's own KDoc).
            Identifier.vanilla("plains") to TreeDecoration.single(TreeConfigs.OAK, 0, 0.05f, 1),
            Identifier.vanilla("sunflower_plains") to TreeDecoration.single(TreeConfigs.OAK, 0, 0.05f, 1),
            // parity: OverworldBiomes.forest(birch=false)/addOtherBirchTrees: TREES_BIRCH_AND_OAK_LEAF_LITTER, oak default.
            Identifier.vanilla("forest") to TreeDecoration.single(TreeConfigs.OAK, 10, 0.1f, 1),
            // parity: OverworldBiomes.forest(flower=true): trees_flower_forest (birch 20%, fallen_birch/fancy_oak
            // dropped, oak default) + flower_flower_forest (real 11-flower NoiseProvider) + default grass.
            Identifier.vanilla("flower_forest") to FlowerForestDecoration,
            // parity: OverworldBiomes.forest(birch=true, tall=false)/addBirchTrees: TREES_BIRCH, birch default.
            Identifier.vanilla("birch_forest") to TreeDecoration.single(TreeConfigs.BIRCH, 10, 0.1f, 1),
            // parity: OverworldBiomes.forest(birch=true, tall=true)/addTallBirchTrees: BIRCH_TALL, super_birch default.
            Identifier.vanilla("old_growth_birch_forest") to TreeDecoration.single(TreeConfigs.SUPER_BIRCH, 10, 0.1f, 1),
            // parity: OverworldBiomes.taiga/addTaigaTrees+addDefaultFlowers: TREES_TAIGA (33.3% pine else spruce,
            // fallen_spruce entry dropped) plus the real default flower patch. Shared ground cover and the
            // taiga mushroom tail are appended by forBiome.
            Identifier.vanilla("taiga") to
                combine(TreeDecoration.choice(TreeConfigs.PINE, 0.33333334f, TreeConfigs.SPRUCE, 10, 0.1f, 1), DefaultVegetationDecoration.FLOWERS),
            // parity: OverworldBiomes.taiga(snowy=true)/addSnowyTrees+addDefaultFlowers: TREES_SNOWY, spruce default.
            Identifier.vanilla("snowy_taiga") to combine(TreeDecoration.single(TreeConfigs.SPRUCE, 0, 0.1f, 1), DefaultVegetationDecoration.FLOWERS),
            // parity: OverworldBiomes.oldGrowthTaiga(spruce=true)/TREES_OLD_GROWTH_SPRUCE_TAIGA: 33.3% mega_spruce,
            // 33.3% pine, fallen_spruce dropped (1.25%), spruce default. countExtra(10, 0.1, 1).
            Identifier.vanilla("old_growth_spruce_taiga") to
                combine(
                    TreeDecoration.sequenceCountExtra(
                        listOf(0.33333334f to TreeConfigs.MEGA_SPRUCE, 0.33333334f to TreeConfigs.PINE, 0.0125f to null),
                        TreeConfigs.SPRUCE,
                        10,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: OverworldBiomes.oldGrowthTaiga(spruce=false)/TREES_OLD_GROWTH_PINE_TAIGA: 2.56% mega_spruce,
            // 30.8% mega_pine, 33.3% pine, fallen_spruce dropped (1.25%), spruce default.
            Identifier.vanilla("old_growth_pine_taiga") to
                combine(
                    TreeDecoration.sequenceCountExtra(
                        listOf(0.025641026f to TreeConfigs.MEGA_SPRUCE, 0.30769232f to TreeConfigs.MEGA_PINE, 0.33333334f to TreeConfigs.PINE, 0.0125f to null),
                        TreeConfigs.SPRUCE,
                        10,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: OverworldBiomes.plains(snowy=true, spikes=false/true)/addSnowyTrees: same TREES_SNOWY feature.
            Identifier.vanilla("snowy_plains") to
                combine(TreeDecoration.single(TreeConfigs.SPRUCE, 0, 0.1f, 1), DefaultVegetationDecoration),
            Identifier.vanilla("ice_spikes") to
                combine(TreeDecoration.single(TreeConfigs.SPRUCE, 0, 0.1f, 1), DefaultVegetationDecoration),
            // parity: OverworldBiomes.savanna/addSavannaTrees+addWarmFlowers: TREES_SAVANNA (80% acacia else oak,
            // fallen_oak entry dropped) plus flower_warm (real flowerDefault, rarity 1/16).
            Identifier.vanilla("savanna") to
                combine(TreeDecoration.choice(TreeConfigs.ACACIA, 0.8f, TreeConfigs.OAK, 1, 0.1f, 1), DefaultVegetationDecoration.WARM_FLOWERS),
            // parity: OverworldBiomes.savanna(plateau=true)/addSavannaTrees+addWarmFlowers: same as plains savanna.
            Identifier.vanilla("savanna_plateau") to
                combine(TreeDecoration.choice(TreeConfigs.ACACIA, 0.8f, TreeConfigs.OAK, 1, 0.1f, 1), DefaultVegetationDecoration.WARM_FLOWERS),
            // parity: OverworldBiomes.savanna(shattered=true)/addShatteredSavannaTrees+addDefaultFlowers (not
            // addWarmFlowers: the shattered variant uses the real default rarity, not the warm one).
            Identifier.vanilla("windswept_savanna") to
                combine(TreeDecoration.choice(TreeConfigs.ACACIA, 0.8f, TreeConfigs.OAK, 2, 0.1f, 1), DefaultVegetationDecoration.FLOWERS),
            // parity: OverworldBiomes.jungle/addJungleTrees+addWarmFlowers+addJungleGrass: TREES_JUNGLE (jungle_tree
            // default, fancy_oak/jungle_bush/mega entries dropped) plus flower_warm and patch_grass_jungle.
            Identifier.vanilla("jungle") to
                combine(
                    BambooDecoration.LIGHT,
                    TreeDecoration.single(TreeConfigs.JUNGLE_TREE, 50, 0.1f, 1),
                    DefaultVegetationDecoration.WARM_FLOWERS,
                    JungleGrassDecoration,
                ),
            // parity: OverworldBiomes.sparseJungle/addSparseJungleTrees+addWarmFlowers+addJungleGrass: TREES_SPARSE_JUNGLE, jungle_tree default.
            Identifier.vanilla("sparse_jungle") to
                combine(TreeDecoration.single(TreeConfigs.JUNGLE_TREE, 2, 0.1f, 1), DefaultVegetationDecoration.WARM_FLOWERS, JungleGrassDecoration),
            // parity: OverworldBiomes.windsweptHills(moreTrees=false)/addMountainTrees+addDefaultFlowers+addDefaultGrass:
            // TREES_WINDSWEPT_HILLS (66.6% spruce else oak) plus the real default grass and flower patches.
            Identifier.vanilla("windswept_hills") to
                TreeDecoration.choice(TreeConfigs.SPRUCE, 0.666f, TreeConfigs.OAK, 0, 0.1f, 1),
            Identifier.vanilla("windswept_gravelly_hills") to
                TreeDecoration.choice(TreeConfigs.SPRUCE, 0.666f, TreeConfigs.OAK, 0, 0.1f, 1),
            // parity: OverworldBiomes.windsweptHills(moreTrees=true)/addMountainForestTrees: TREES_WINDSWEPT_FOREST, same species mix.
            Identifier.vanilla("windswept_forest") to
                TreeDecoration.choice(TreeConfigs.SPRUCE, 0.666f, TreeConfigs.OAK, 3, 0.1f, 1),
            // parity: OverworldBiomes.grove/addGroveTrees: TREES_GROVE, 33.3% pine else spruce (real "on snow" placement filter dropped).
            Identifier.vanilla("grove") to TreeDecoration.choice(TreeConfigs.PINE, 0.33333334f, TreeConfigs.SPRUCE, 10, 0.1f, 1),
            // parity: peaks have no vegetal features; snowy slopes adds only patch_pumpkin later.
            // Explicit empty bases keep their earlier shared disk stage reachable through forBiome.
            Identifier.vanilla("snowy_slopes") to noVegetalDecoration,
            Identifier.vanilla("frozen_peaks") to noVegetalDecoration,
            Identifier.vanilla("jagged_peaks") to noVegetalDecoration,
            Identifier.vanilla("stony_peaks") to noVegetalDecoration,
            // parity: OverworldBiomes.badlands(wooded=true)/addBadlandsTrees+addBadlandGrass: TREES_BADLANDS (oak
            // default, oak_leaf_litter/fallen_oak simplified to plain oak) plus patch_grass_badlands (the same
            // real feature DefaultVegetationDecoration.GRASS already builds) and patch_dead_bush_badlands.
            Identifier.vanilla("wooded_badlands") to combine(TreeDecoration.single(TreeConfigs.OAK, 5, 0.1f, 1), DefaultVegetationDecoration.GRASS, BadlandsDecoration),
            // parity: OverworldBiomes.badlands(wooded=false)/addBadlandGrass; specialized ground cover is appended by forBiome.
            Identifier.vanilla("badlands") to combine(DefaultVegetationDecoration.GRASS, BadlandsDecoration),
            Identifier.vanilla("eroded_badlands") to combine(DefaultVegetationDecoration.GRASS, BadlandsDecoration),
            // parity: OverworldBiomes.swamp/addSwampVegetation: TREES_SWAMP (swamp_oak, a single feature, not a
            // random selector) plus flower_swamp/patch_grass_normal/patch_dead_bush/patch_waterlily.
            Identifier.vanilla("swamp") to combine(TreeDecoration.single(TreeConfigs.SWAMP_OAK, 2, 0.1f, 1), SwampDecoration),
            // parity: OverworldBiomes.desert/addDesertVegetation+addDesertExtraVegetation: patch_cactus_desert, patch_dead_bush_2.
            Identifier.vanilla("desert") to DesertDecoration,
            // parity: OverworldBiomes.darkForest(isPaleGarden=false)/DARK_FOREST_VEGETATION: real RANDOM_SELECTOR,
            // huge mushrooms dropped (2.5%/5%), dark_oak 66.7%, fallen_birch dropped (0.25%), birch 20%, fallen_oak
            // dropped (1.25%), fancy_oak dropped (10%), oak default. CountPlacement(16), not countExtra.
            Identifier.vanilla("dark_forest") to
                TreeDecoration.sequence(
                    listOf(0.025f to null, 0.05f to null, 0.6666667f to TreeConfigs.DARK_OAK, 0.0025f to null, 0.2f to TreeConfigs.BIRCH, 0.0125f to null, 0.1f to null),
                    TreeConfigs.OAK,
                    16,
                ),
            // parity: OverworldBiomes.darkForest(isPaleGarden=true)/PALE_GARDEN_VEGETATION: 10% pale_oak_creaking
            // (same tree shape as the default, only the CreakingHeartDecorator differs, not ported), else pale_oak.
            // CountPlacement(16). Pale moss and pale garden flowers are appended by forBiome.
            Identifier.vanilla("pale_garden") to TreeDecoration.fixed(TreeConfigs.PALE_OAK, 16),
            // parity: meadow tall grass, grass, DualNoiseProvider flowers, and wildflowers.
            // The rare trees_meadow selector remains documented in MeadowDecoration.
            Identifier.vanilla("meadow") to MeadowDecoration,
            // parity: OverworldBiomes.meadowOrCherryGrove(cherryGrove=true)/addCherryGroveVegetation:
            // patch_grass_plain + flower_cherry (pink petals) + trees_cherry (countExtra(10, 0.1, 1),
            // cherry_bees_005 minus the unported BeehiveDecorator). See CherryGroveDecoration's own KDoc.
            Identifier.vanilla("cherry_grove") to CherryGroveDecoration,
            // parity: OverworldBiomes.river/addDefaultFlowers+addDefaultGrass (both frozen and unfrozen call this
            // unconditionally), followed by bushes and seagrass_river only when unfrozen. Water trees remain unported.
            Identifier.vanilla("river") to combine(DefaultVegetationDecoration, AquaticDecoration.RIVER),
            Identifier.vanilla("frozen_river") to DefaultVegetationDecoration,
            // parity: OverworldBiomes.beach(snowy=false, stony=false)/addDefaultFlowers+addDefaultGrass.
            Identifier.vanilla("beach") to DefaultVegetationDecoration,
            Identifier.vanilla("snowy_beach") to DefaultVegetationDecoration,
            Identifier.vanilla("stony_shore") to DefaultVegetationDecoration,
            // parity: OverworldBiomes.bambooJungle/baseJungle: addWarmFlowers+addJungleGrass (shared with
            // jungle/sparse_jungle), after the real dense bamboo patch. The separate
            // bamboo_vegetation random-selector's mega jungle trees remain a documented gap.
            Identifier.vanilla("bamboo_jungle") to
                combine(BambooDecoration.DENSE, DefaultVegetationDecoration.WARM_FLOWERS, JungleGrassDecoration),
            // parity: OverworldBiomes.dripstoneCaves/deepDark: both call addPlainGrass+addPlainVegetation, the
            // exact same real features Plains itself uses (PlainsDecoration already builds them). Real vanilla
            // also runs addDripstone/addSculk here (cave-specific structures, not ported); this project has no
            // cave generation yet either, so these two biomes are effectively unreachable at the surface right
            // now, but the real ground cover they'd get once caves exist is already correct.
            Identifier.vanilla("dripstone_caves") to combine(TreeDecoration.single(TreeConfigs.OAK, 0, 0.05f, 1), PlainsDecoration),
            Identifier.vanilla("deep_dark") to combine(TreeDecoration.single(TreeConfigs.OAK, 0, 0.05f, 1), PlainsDecoration),
            // parity: OverworldBiomes.lushCaves/sulfurCaves: both call only addPlainGrass (real patch_tall_grass_2),
            // not the rest of Plains' own decoration. addLushCavesVegetationFeatures/addSulfurCavesFeatures (cave
            // ceiling vegetation, glow berries, azalea, sulfur spikes) are real cave-specific systems, not ported;
            // like dripstone_caves/deep_dark, neither biome is reachable without cave generation yet either.
            Identifier.vanilla("lush_caves") to PlainsDecoration.TALL_GRASS,
            Identifier.vanilla("sulfur_caves") to PlainsDecoration.TALL_GRASS,
            // parity: OverworldBiomes.mangroveSwamp/addMangroveSwampVegetation: TREES_MANGROVE (85% tall_mangrove
            // else mangrove, CountPlacement(25) fixed) plus patch_grass_normal/patch_dead_bush/patch_waterlily
            // (the same real features SwampDecoration.GROUND_COVER already builds, no flower_swamp here).
            // Real MangroveRootPlacer ported: trunks start elevated on simulated roots.
            // SurfaceWaterDepthFilter not ported (no fluid modeling, an established gap).
            Identifier.vanilla("mangrove_swamp") to
                combine(TreeDecoration.sequence(listOf(0.85f to TreeConfigs.TALL_MANGROVE), TreeConfigs.MANGROVE, 25), SwampDecoration.GROUND_COVER),
            // parity: OverworldBiomes.mushroomFields/addMushroomFieldVegetation: mushroom_island_vegetation
            // (huge red/brown 50/50, one attempt per chunk) + brown_mushroom_taiga (rarity 4) +
            // red_mushroom_taiga (rarity 256). Shared near-water vegetation is appended by forBiome.
            Identifier.vanilla("mushroom_fields") to MushroomFieldsDecoration,
            // parity: OverworldBiomes.ocean/coldOcean/lukeWarmOcean. Each variant selects the
            // matching real seagrass count/tall probability, then its cold or warm kelp density.
            Identifier.vanilla("ocean") to AquaticDecoration.OCEAN,
            Identifier.vanilla("deep_ocean") to AquaticDecoration.DEEP_OCEAN,
            Identifier.vanilla("cold_ocean") to AquaticDecoration.COLD_OCEAN,
            Identifier.vanilla("deep_cold_ocean") to AquaticDecoration.DEEP_COLD_OCEAN,
            Identifier.vanilla("lukewarm_ocean") to AquaticDecoration.LUKEWARM_OCEAN,
            Identifier.vanilla("deep_lukewarm_ocean") to AquaticDecoration.DEEP_LUKEWARM_OCEAN,
            // parity: warm ocean adds coral first, then seagrass_warm and sea_pickle.
            Identifier.vanilla("warm_ocean") to AquaticDecoration.WARM_OCEAN,
            // parity: frozenOcean adds packed/blue icebergs in LOCAL_MODIFICATIONS, blue ice in
            // SURFACE_STRUCTURES, then the same default flower/grass/mushroom features as beaches.
            // Water trees remain unported.
            Identifier.vanilla("frozen_ocean") to DefaultVegetationDecoration,
            Identifier.vanilla("deep_frozen_ocean") to DefaultVegetationDecoration,
        )

    public fun forBiome(id: Identifier): BiomeVegetalDecoration? {
        val path = id.path
        val body = when (path) {
            "plains" ->
                combine(
                    PlainsDecoration.TALL_GRASS,
                    ForestGroundDecoration.BUSH,
                    requireBase(id),
                    PlainsDecoration.FLOWERS,
                    PlainsDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "sunflower_plains" ->
                combine(
                    PlainsDecoration.TALL_GRASS,
                    PlainsDecoration.SUNFLOWER,
                    requireBase(id),
                    PlainsDecoration.FLOWERS,
                    PlainsDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "ocean" -> oceanWithExtra(AquaticDecoration.OCEAN)
            "deep_ocean" -> oceanWithExtra(AquaticDecoration.DEEP_OCEAN)
            "cold_ocean" -> oceanWithExtra(AquaticDecoration.COLD_OCEAN)
            "deep_cold_ocean" -> oceanWithExtra(AquaticDecoration.DEEP_COLD_OCEAN)
            "lukewarm_ocean" -> oceanWithExtra(AquaticDecoration.LUKEWARM_OCEAN)
            "deep_lukewarm_ocean" -> oceanWithExtra(AquaticDecoration.DEEP_LUKEWARM_OCEAN)
            "warm_ocean" -> oceanWithExtra(AquaticDecoration.WARM_OCEAN)
            "river" ->
                combine(
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                    AquaticDecoration.RIVER,
                )
            "frozen_river" ->
                combine(
                    ForestGroundDecoration.BUSH,
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "swamp" ->
                combine(
                    requireBase(id),
                    MushroomDecoration.SWAMP,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.SWAMP,
                    AquaticDecoration.SWAMP,
                )
            "mangrove_swamp" ->
                combine(requireBase(id), AquaticDecoration.SWAMP, ExtraVegetationDecoration.MANGROVE)
            "desert" -> combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.DESERT)
            "badlands", "eroded_badlands", "wooded_badlands" ->
                combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.BADLANDS)
            "mushroom_fields" -> combine(requireBase(id), ExtraVegetationDecoration.NEAR_WATER)
            "jungle", "bamboo_jungle" ->
                combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.DEFAULT, JungleMelonDecoration.NORMAL)
            "sparse_jungle" ->
                combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.DEFAULT, JungleMelonDecoration.SPARSE)
            "savanna", "savanna_plateau" ->
                combine(
                    SavannaGroundDecoration.TALL_GRASS,
                    requireBase(id),
                    SavannaGroundDecoration.SAVANNA,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "windswept_savanna" ->
                combine(
                    requireBase(id),
                    SavannaGroundDecoration.WINDSWEPT,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "forest" ->
                combine(
                    ForestFlowerDecoration.NORMAL,
                    requireBase(id),
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration.FLOWERS,
                    ForestGroundDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "birch_forest", "old_growth_birch_forest" ->
                combine(
                    ForestFlowerDecoration.NORMAL,
                    ForestFloorDecoration.BIRCH_WILDFLOWERS,
                    requireBase(id),
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration.FLOWERS,
                    ForestGroundDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "dark_forest" ->
                combine(
                    requireBase(id),
                    ForestFlowerDecoration.NORMAL,
                    DefaultVegetationDecoration.FLOWERS,
                    ForestGroundDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ForestFloorDecoration.LEAF_LITTER,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "pale_garden" ->
                combine(
                    requireBase(id),
                    PaleMossDecoration,
                    PaleGardenDecoration.FLOWER_PATCH,
                    PaleGardenDecoration.RARE_FLOWER,
                    ForestGroundDecoration.GRASS,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "flower_forest" ->
                combine(
                    ForestFlowerDecoration.FLOWER_FOREST,
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "windswept_hills", "windswept_gravelly_hills", "windswept_forest" ->
                combine(
                    requireBase(id),
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            in OLD_GROWTH_TAIGA_PATHS ->
                combine(
                    TaigaGroundDecoration.LARGE_FERN,
                    requireBase(id),
                    TaigaGroundDecoration.OLD_GROWTH,
                    MushroomDecoration.OLD_GROWTH,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                    TaigaGroundDecoration.COMMON_BERRY,
                )
            "taiga" ->
                combine(
                    TaigaGroundDecoration.LARGE_FERN,
                    requireBase(id),
                    TaigaGroundDecoration.NORMAL,
                    MushroomDecoration.TAIGA,
                    ExtraVegetationDecoration.DEFAULT,
                    TaigaGroundDecoration.COMMON_BERRY,
                )
            "snowy_taiga" ->
                combine(
                    TaigaGroundDecoration.LARGE_FERN,
                    requireBase(id),
                    TaigaGroundDecoration.NORMAL,
                    MushroomDecoration.TAIGA,
                    ExtraVegetationDecoration.DEFAULT,
                    TaigaGroundDecoration.RARE_BERRY,
                )
            in DEFAULT_EXTRA_PATHS ->
                if (path == "pale_garden") {
                    combine(requireBase(id), ExtraVegetationDecoration.DEFAULT)
                } else {
                    combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.DEFAULT)
                }
            in PUMPKIN_ONLY_PATHS ->
                if (path == "dripstone_caves" || path == "deep_dark") {
                    combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.PUMPKIN_ONLY)
                } else {
                    combine(requireBase(id), ExtraVegetationDecoration.PUMPKIN_ONLY)
                }
            else -> byBiome[id]
        }
        body ?: return null
        return when (path) {
            "ice_spikes" -> combine(IceSpikesDecoration, SoftDiskDecoration.DEFAULT, GlowLichenDecoration, body)
            "frozen_ocean", "deep_frozen_ocean" -> combine(FrozenOceanDecoration, SoftDiskDecoration.DEFAULT, GlowLichenDecoration, body)
            "swamp" -> combine(SoftDiskDecoration.SWAMP, GlowLichenDecoration, body)
            "mangrove_swamp" -> combine(SoftDiskDecoration.MANGROVE, GlowLichenDecoration, body)
            else -> combine(SoftDiskDecoration.DEFAULT, GlowLichenDecoration, body)
        }
    }

    private fun oceanWithExtra(aquatic: BiomeVegetalDecoration): BiomeVegetalDecoration =
        combine(DefaultVegetationDecoration, MushroomDecoration.NORMAL, ExtraVegetationDecoration.DEFAULT, aquatic)

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
