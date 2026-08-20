package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/**
 * Global step-9 placement for the large tree/forest features.
 *
 * Unlike the old chunk-centre biome dispatch, each placed feature owns its official seed/index and
 * applies the final biome filter at the randomized origin. A tree may still grow across the biome
 * boundary, which is vanilla behavior; only its origin decides whether the placed feature runs.
 */
public object TreePatchDecoration {
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
        placementObserver: ((name: String, position: DecorationPos) -> Unit)? = null,
    ) {
        decorateSpecs(chunk, seed, environment, listOf(SPECS.single { it.name == name }), placementObserver)
    }

    internal fun featureSpecs(): List<FeatureSpec> = SPECS

    private fun decorateSpecs(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        specs: List<FeatureSpec>,
        placementObserver: ((name: String, position: DecorationPos) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val context = PlacementContext(chunk)
        val origin = DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16)
        for (spec in specs) {
            random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
            var positions = spec.frequency.getPositions(context, random, origin)
            positions = positions.flatMap { InSquarePlacement.getPositions(context, random, it) }
            positions =
                positions.flatMap {
                    SurfaceWaterDepthFilter(spec.maxWaterDepth).getPositions(context, random, it)
                }
            positions =
                positions.flatMap {
                    HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR).getPositions(context, random, it)
                }
            positions.forEach { position ->
                val biome = environment.biomeAt(position.x, position.y, position.z).substringAfter(':')
                if (biome !in spec.biomes) return@forEach
                if (!survivesPlacementPredicate(chunk, position, spec.survivalSapling)) return@forEach
                placementObserver?.invoke(spec.name, position)
                if (spec.name == BAMBOO_VEGETATION) {
                    BambooVegetationDecoration.placeAt(chunk, random, position, context)
                } else {
                    TreeDecoration.placeAt(chunk, random, position, spec.name)
                }
            }
        }
    }

    internal data class FeatureSpec(
        val name: String,
        val index: Int,
        val frequency: PlacementModifier,
        val biomes: Set<String>,
        val maxWaterDepth: Int = 0,
        val survivalSapling: String? = null,
    )

    /** The optional outer `would_survive` filters use an actual stage-zero sapling state. */
    private fun survivesPlacementPredicate(
        chunk: Chunk,
        position: DecorationPos,
        sapling: String?,
    ): Boolean {
        if (sapling == null) return true
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        val stateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(sapling))?.stateId
                ?: error("unknown sapling '$sapling'")

        fun get(
            dx: Int,
            dy: Int,
            dz: Int,
        ): Int = chunk.getDecorationBlockStateId(position.x + dx, position.y + dy, position.z + dz)

        return BlockSupport.canSurvive(
            stateId,
            down = get(0, -1, 0),
            up = get(0, 1, 0),
            north = get(0, 0, -1),
            south = get(0, 0, 1),
            west = get(-1, 0, 0),
            east = get(1, 0, 0),
        )
    }

    private fun countExtra(
        count: Int,
        chance: Float,
        extra: Int,
    ): PlacementModifier = CountExtraPlacement(count, chance, extra)

    private val SPECS =
        listOf(
            FeatureSpec("trees_windswept_savanna", 1, countExtra(2, 0.1f, 1), setOf("windswept_savanna")),
            FeatureSpec(
                BAMBOO_VEGETATION,
                3,
                CountPlacement(IntProvider.Weighted(30 to 9, 31 to 1)),
                setOf("bamboo_jungle"),
            ),
            FeatureSpec("trees_sparse_jungle", 4, countExtra(2, 0.1f, 1), setOf("sparse_jungle")),
            FeatureSpec(
                "trees_badlands",
                5,
                countExtra(5, 0.1f, 1),
                setOf("wooded_badlands"),
                survivalSapling = "oak_sapling",
            ),
            FeatureSpec("trees_jungle", 7, countExtra(50, 0.1f, 1), setOf("jungle")),
            FeatureSpec("trees_savanna", 9, countExtra(1, 0.1f, 1), setOf("savanna", "savanna_plateau")),
            FeatureSpec("pale_garden_vegetation", 13, CountPlacement(16), setOf("pale_garden")),
            FeatureSpec("dark_forest_vegetation", 17, CountPlacement(16), setOf("dark_forest")),
            FeatureSpec("trees_flower_forest", 19, countExtra(6, 0.1f, 1), setOf("flower_forest")),
            FeatureSpec("birch_tall", 23, countExtra(10, 0.1f, 1), setOf("old_growth_birch_forest")),
            FeatureSpec(
                "trees_birch",
                24,
                countExtra(10, 0.1f, 1),
                setOf("birch_forest"),
                survivalSapling = "birch_sapling",
            ),
            FeatureSpec("trees_birch_and_oak_leaf_litter", 25, countExtra(10, 0.1f, 1), setOf("forest")),
            FeatureSpec("trees_meadow", 37, RarityFilter(100), setOf("meadow")),
            FeatureSpec("trees_windswept_forest", 39, countExtra(3, 0.1f, 1), setOf("windswept_forest")),
            FeatureSpec(
                "trees_old_growth_pine_taiga",
                41,
                countExtra(10, 0.1f, 1),
                setOf("old_growth_pine_taiga"),
            ),
            FeatureSpec(
                "trees_old_growth_spruce_taiga",
                42,
                countExtra(10, 0.1f, 1),
                setOf("old_growth_spruce_taiga"),
            ),
            FeatureSpec("trees_taiga", 43, countExtra(10, 0.1f, 1), setOf("snowy_taiga", "taiga")),
            FeatureSpec("trees_grove", 44, countExtra(10, 0.1f, 1), setOf("grove")),
            FeatureSpec(
                "trees_windswept_hills",
                45,
                countExtra(0, 0.1f, 1),
                setOf("windswept_gravelly_hills", "windswept_hills"),
            ),
            FeatureSpec(
                "trees_snowy",
                46,
                countExtra(0, 0.1f, 1),
                setOf("ice_spikes", "snowy_plains"),
                survivalSapling = "spruce_sapling",
            ),
            FeatureSpec("trees_mangrove", 47, CountPlacement(25), setOf("mangrove_swamp"), maxWaterDepth = 5),
            FeatureSpec(
                "trees_swamp",
                48,
                countExtra(2, 0.1f, 1),
                setOf("swamp"),
                maxWaterDepth = 2,
                survivalSapling = "oak_sapling",
            ),
            FeatureSpec(
                "trees_water",
                50,
                CountPlacement(IntProvider.Weighted(0 to 9, 1 to 1)),
                setOf(
                    "cold_ocean",
                    "deep_cold_ocean",
                    "deep_frozen_ocean",
                    "deep_lukewarm_ocean",
                    "deep_ocean",
                    "frozen_ocean",
                    "frozen_river",
                    "lukewarm_ocean",
                    "ocean",
                    "river",
                    "warm_ocean",
                ),
            ),
            FeatureSpec(
                "trees_plains",
                52,
                countExtra(0, 0.05f, 1),
                setOf("deep_dark", "dripstone_caves", "plains", "sunflower_plains"),
                survivalSapling = "oak_sapling",
            ),
            FeatureSpec(
                "trees_cherry",
                56,
                countExtra(10, 0.1f, 1),
                setOf("cherry_grove"),
                survivalSapling = "cherry_sapling",
            ),
        )

    private const val BAMBOO_VEGETATION = "bamboo_vegetation"
    private const val VEGETAL_DECORATION_STEP = 9
}
