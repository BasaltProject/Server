package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/** Independently seeded step-9 placement for the remaining surface flower features. */
public object FlowerPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        decorateSpecs(chunk, seed, environment, SPECS, null)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        name: String,
        placementObserver: ((DecorationPos) -> Unit)? = null,
    ) {
        decorateSpecs(chunk, seed, environment, listOf(SPECS.single { it.name == name }), placementObserver)
    }

    internal fun featureSpecs(): List<FeatureSpec> = SPECS

    private fun decorateSpecs(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        specs: List<FeatureSpec>,
        placementObserver: ((DecorationPos) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val context = PlacementContext(chunk)
        val chunkOrigin = DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16)
        for (spec in specs) {
            random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
            var positions: Sequence<DecorationPos> = sequenceOf(chunkOrigin)
            for (modifier in spec.beforeBiome) {
                positions = positions.flatMap { modifier.getPositions(context, random, it) }
            }
            positions.forEach { position ->
                val biome = environment.biomeAt(position.x, position.y, position.z).substringAfter(':')
                if (biome !in spec.biomes) return@forEach
                placementObserver?.invoke(position)
                spec.place(chunk, random, position)
            }
        }
    }

    internal data class FeatureSpec(
        val name: String,
        val index: Int,
        val beforeBiome: List<PlacementModifier>,
        val biomes: Set<String>,
        val place: (Chunk, WorldgenRandom, DecorationPos) -> Unit,
    )

    private fun surface(vararg prefix: PlacementModifier): List<PlacementModifier> =
        prefix.toList() + InSquarePlacement + HeightmapPlacement

    private fun surface(
        heightmap: Chunk.HeightmapType,
        vararg prefix: PlacementModifier,
    ): List<PlacementModifier> = prefix.toList() + InSquarePlacement + HeightmapPlacement(heightmap)

    private val SPECS =
        listOf(
            FeatureSpec(
                "flower_warm",
                10,
                surface(RarityFilter(16)),
                setOf("bamboo_jungle", "jungle", "savanna", "savanna_plateau", "sparse_jungle"),
                DefaultVegetationDecoration::placeDefaultPatchAt,
            ),
            FeatureSpec(
                "pale_garden_flowers",
                15,
                surface(Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES, RarityFilter(8)),
                setOf("pale_garden"),
                PaleGardenDecoration::placeAt,
            ),
            FeatureSpec(
                "flower_pale_garden",
                16,
                surface(RarityFilter(32)),
                setOf("pale_garden"),
                PaleGardenDecoration::placeSingleAt,
            ),
            FeatureSpec(
                "flower_forest_flowers",
                18,
                listOf(
                    RarityFilter(7),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(IntProvider.Clamped(IntProvider.Uniform(-1, 3), min = 0, max = 3)),
                ),
                setOf("flower_forest"),
                ForestFlowerDecoration::placeAt,
            ),
            FeatureSpec(
                "flower_flower_forest",
                20,
                surface(CountPlacement(3), RarityFilter(2)),
                setOf("flower_forest"),
                FlowerForestDecoration::placeAt,
            ),
            FeatureSpec(
                "forest_flowers",
                21,
                listOf(
                    RarityFilter(7),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(IntProvider.Clamped(IntProvider.Uniform(-3, 1), min = 0, max = 1)),
                ),
                setOf("birch_forest", "dark_forest", "forest", "old_growth_birch_forest"),
                ForestFlowerDecoration::placeAt,
            ),
            FeatureSpec(
                "wildflowers_birch_forest",
                22,
                surface(CountPlacement(3), RarityFilter(2)),
                setOf("birch_forest", "old_growth_birch_forest"),
                place = { chunk, random, origin ->
                    ForestFloorDecoration.placeWildflowersAt(chunk, random, origin, 64)
                },
            ),
            FeatureSpec(
                "flower_meadow",
                36,
                surface(),
                setOf("meadow"),
                MeadowDecoration::placeAt,
            ),
            FeatureSpec(
                "wildflowers_meadow",
                38,
                surface(NoiseThresholdCountPlacement(-0.8, belowNoise = 5, aboveNoise = 10)),
                setOf("meadow"),
                place = { chunk, random, origin ->
                    ForestFloorDecoration.placeWildflowersAt(chunk, random, origin, 8)
                },
            ),
            FeatureSpec(
                "flower_swamp",
                49,
                surface(RarityFilter(32)),
                setOf("swamp"),
                SwampDecoration::placeAt,
            ),
            FeatureSpec(
                "flower_plains",
                53,
                surface(
                    NoiseThresholdCountPlacement(-0.8, belowNoise = 15, aboveNoise = 4),
                    RarityFilter(32),
                ),
                setOf("deep_dark", "dripstone_caves", "plains", "sunflower_plains"),
                place = { chunk, random, origin ->
                    PlainsDecoration.placeFlowerPlainAt(chunk, random, origin.x, origin.y, origin.z)
                },
            ),
            FeatureSpec(
                "flower_cherry",
                55,
                surface(NoiseThresholdCountPlacement(-0.8, belowNoise = 5, aboveNoise = 10)),
                setOf("cherry_grove"),
                CherryGroveDecoration::placeAt,
            ),
            FeatureSpec(
                "flower_default",
                57,
                surface(RarityFilter(32)),
                setOf(
                    "beach",
                    "birch_forest",
                    "cold_ocean",
                    "dark_forest",
                    "deep_cold_ocean",
                    "deep_frozen_ocean",
                    "deep_lukewarm_ocean",
                    "deep_ocean",
                    "desert",
                    "forest",
                    "frozen_ocean",
                    "frozen_river",
                    "ice_spikes",
                    "lukewarm_ocean",
                    "ocean",
                    "old_growth_birch_forest",
                    "old_growth_pine_taiga",
                    "old_growth_spruce_taiga",
                    "river",
                    "snowy_beach",
                    "snowy_plains",
                    "snowy_taiga",
                    "stony_shore",
                    "taiga",
                    "warm_ocean",
                    "windswept_forest",
                    "windswept_gravelly_hills",
                    "windswept_hills",
                    "windswept_savanna",
                ),
                DefaultVegetationDecoration::placeDefaultPatchAt,
            ),
        )

    private const val VEGETAL_DECORATION_STEP = 9
}
