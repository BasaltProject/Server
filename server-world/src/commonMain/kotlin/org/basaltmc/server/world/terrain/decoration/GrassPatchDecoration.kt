package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** // parity: the nine short-grass/fern patch placed features in VEGETAL_DECORATION. */
public object GrassPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        for (spec in SPECS) {
            decorateFeature(spec.index, chunk, seed, environment, externalBlockState, blockWriteObserver)
        }
    }

    internal fun decorateFeature(
        index: Int,
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val spec = SPECS.singleOrNull { it.index == index } ?: error("unknown grass vegetal index $index")
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val context = PlacementContext(chunk)
        val region = GrassRegion(chunk, externalBlockState, blockWriteObserver)
        random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
        place(context, region, random, environment, spec)
    }

    private fun place(
        context: PlacementContext,
        region: GrassRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
        spec: GrassSpec,
    ) {
        val chunkOrigin = DecorationPos(context.chunk.chunkX * 16, 0, context.chunk.chunkZ * 16)
        val outerCount =
            spec.noiseCount?.getPositions(context, random, chunkOrigin)?.count()
                ?: spec.outerCount
        repeat(outerCount) {
            val x = context.chunk.chunkX * 16 + random.nextInt(16)
            val z = context.chunk.chunkZ * 16 + random.nextInt(16)
            val origin = DecorationPos(x, context.height(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE), z)
            val biome = environment.biomeAt(origin.x, origin.y, origin.z).substringAfter(':')
            if (biome in spec.biomes) placePatch(region, random, origin, spec)
        }
    }

    /**
     * The vanilla placed feature names of the grass patches this step runs, by the spec that draws
     * them: the name says which provider and how many tries, the biomes decide where generation
     * puts one and `/place feature` is told instead.
     */
    public val PLACED_GRASS_IDS: Map<String, Int> =
        mapOf(
            "patch_grass_jungle" to 11,
            "patch_grass_savanna" to 12,
            "patch_grass_meadow" to 35,
            "patch_grass_plain" to 54,
            "patch_taiga_grass" to 58,
            "patch_grass_forest" to 59,
            "patch_grass_taiga" to 60,
            "patch_grass_taiga_2" to 60,
            "patch_grass_normal" to 61,
            "patch_grass_badlands" to 68,
        )

    /** The patch at one origin, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean {
        val index = PLACED_GRASS_IDS[placedFeature] ?: return false
        val spec = SPECS.firstOrNull { it.index == index } ?: return false
        placePatch(GrassRegion(chunk, null, null), random, origin, spec)
        return true
    }

    private fun placePatch(
        region: GrassRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
        spec: GrassSpec,
    ) {
        repeat(spec.innerCount) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return@repeat
            if (spec.rejectPodzol &&
                Blocks.byStateId(region.get(target.down()))?.block == Blocks.PODZOL.block
            ) {
                return@repeat
            }
            val stateId = spec.provider.state(random)
            if (target.y == WorldHeight.MIN_Y ||
                Blocks.byStateId(region.get(target.down()))?.block?.path !in SUPPORTS_VEGETATION
            ) {
                return@repeat
            }
            region.set(target, stateId)
        }
    }

    private fun DecorationPos.down(): DecorationPos = copy(y = y - 1)

    private class GrassRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else ->
                    externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
                        ?: Blocks.AIR.stateId
            }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                setExternalDecorationBlockStateId(chunk, externalWrites, externalBlockState, position, stateId)
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private enum class Provider {
        GRASS,
        TAIGA,
        JUNGLE,
        ;

        fun state(random: WorldgenRandom): Int =
            when (this) {
                GRASS -> SHORT_GRASS_STATE_ID
                TAIGA -> if (random.nextInt(5) == 0) SHORT_GRASS_STATE_ID else FERN_STATE_ID
                JUNGLE -> if (random.nextInt(4) < 3) SHORT_GRASS_STATE_ID else FERN_STATE_ID
            }
    }

    private data class GrassSpec(
        val index: Int,
        val outerCount: Int,
        val innerCount: Int,
        val biomes: Set<String>,
        val provider: Provider = Provider.GRASS,
        val noiseCount: NoiseThresholdCountPlacement? = null,
        val rejectPodzol: Boolean = false,
    )

    private val SUPPORTS_VEGETATION =
        setOf(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
            "farmland",
        )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val SHORT_GRASS_STATE_ID = stateId("short_grass")
    private val FERN_STATE_ID = stateId("fern")
    private val SPECS =
        listOf(
            GrassSpec(
                11,
                25,
                32,
                setOf("bamboo_jungle", "jungle", "sparse_jungle"),
                Provider.JUNGLE,
                rejectPodzol = true,
            ),
            GrassSpec(12, 20, 32, setOf("savanna", "savanna_plateau")),
            GrassSpec(35, 0, 16, setOf("meadow"), noiseCount = NoiseThresholdCountPlacement(-0.8, 5, 10)),
            GrassSpec(
                54,
                0,
                32,
                setOf("cherry_grove", "deep_dark", "dripstone_caves", "plains", "sunflower_plains"),
                noiseCount = NoiseThresholdCountPlacement(-0.8, 5, 10),
            ),
            GrassSpec(58, 7, 32, setOf("old_growth_pine_taiga", "old_growth_spruce_taiga"), Provider.TAIGA),
            GrassSpec(
                59,
                2,
                32,
                setOf("birch_forest", "dark_forest", "forest", "old_growth_birch_forest", "pale_garden"),
            ),
            GrassSpec(60, 1, 32, setOf("snowy_taiga", "taiga"), Provider.TAIGA),
            GrassSpec(61, 5, 32, setOf("mangrove_swamp", "swamp", "windswept_savanna")),
            GrassSpec(
                68,
                1,
                32,
                setOf(
                    "badlands",
                    "beach",
                    "cold_ocean",
                    "deep_cold_ocean",
                    "deep_frozen_ocean",
                    "deep_lukewarm_ocean",
                    "deep_ocean",
                    "desert",
                    "eroded_badlands",
                    "flower_forest",
                    "frozen_ocean",
                    "frozen_river",
                    "ice_spikes",
                    "lukewarm_ocean",
                    "ocean",
                    "river",
                    "snowy_beach",
                    "snowy_plains",
                    "stony_shore",
                    "warm_ocean",
                    "windswept_forest",
                    "windswept_gravelly_hills",
                    "windswept_hills",
                    "wooded_badlands",
                ),
            ),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
