package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the normal and sparse jungle melon patches. */
public object MelonPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateSpecs(chunk, seed, environment, SPECS, externalBlockState, blockWriteObserver)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        name: String,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateSpecs(
            chunk,
            seed,
            environment,
            listOf(SPECS.single { it.name == name }),
            externalBlockState,
            blockWriteObserver,
        )
    }

    private fun decorateSpecs(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        specs: List<PatchSpec>,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val context = PlacementContext(chunk)
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)
        for (spec in specs) {
            random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
            if (random.nextFloat() >= 1.0f / spec.rarity) continue
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) continue
            placeAt(random, DecorationPos(x, y, z), region)
        }
    }

    /** The patch at one origin: what the step calls after choosing a spot, and what `/place feature` calls. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        placeAt(random, origin, PatchRegion(chunk, null, null))
    }

    private fun placeAt(
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(64) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return@repeat
            val current = region.get(target)
            val currentPath = Blocks.byStateId(current)?.block?.path
            if (currentPath !in REPLACEABLE_BLOCKS || Blocks.hasFluid(current)) return@repeat
            if (target.y == WorldHeight.MIN_Y ||
                Blocks.byStateId(region.get(target.copy(y = target.y - 1)))?.block != Blocks.GRASS_BLOCK.block
            ) {
                return@repeat
            }
            region.set(target, MELON_STATE_ID)
        }
    }

    private class PatchRegion(
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

    private data class PatchSpec(
        val name: String,
        val index: Int,
        val rarity: Int,
        val biomes: Set<String>,
    )

    private val MELON_STATE_ID =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("melon"))?.stateId
            ?: error("unknown block 'melon'")
    private val REPLACEABLE_BLOCKS =
        setOf(
            "air",
            "water",
            "lava",
            "short_grass",
            "fern",
            "dead_bush",
            "bush",
            "short_dry_grass",
            "tall_dry_grass",
            "seagrass",
            "tall_seagrass",
            "fire",
            "soul_fire",
            "snow",
            "vine",
            "glow_lichen",
            "resin_clump",
            "light",
            "tall_grass",
            "large_fern",
            "structure_void",
            "void_air",
            "cave_air",
            "bubble_column",
            "warped_roots",
            "nether_sprouts",
            "crimson_roots",
            "leaf_litter",
            "hanging_roots",
        )
    private val SPECS =
        listOf(
            PatchSpec("patch_melon_sparse", 92, 64, setOf("sparse_jungle")),
            PatchSpec("patch_melon", 93, 6, setOf("bamboo_jungle", "jungle")),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
