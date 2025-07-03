package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the desert and decorated badlands cactus patches. */
public object CactusPatchDecoration {
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

    private fun placeAt(
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(10) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            if (!canPlaceCactus(region, target)) return@repeat
            CactusFeature.placeColumn(region, random, target)
        }
    }

    private fun canPlaceCactus(
        region: PatchRegion,
        position: DecorationPos,
    ): Boolean {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(position))) return false
        for ((dx, dz) in HORIZONTAL_OFFSETS) {
            val neighbour = region.get(position.copy(x = position.x + dx, z = position.z + dz))
            if (BlockShapes.isSolidState(neighbour) || neighbour in LAVA_STATE_IDS) return false
        }
        if (position.y == WorldHeight.MIN_Y ||
            Blocks.byStateId(region.get(position.copy(y = position.y - 1)))?.block?.path !in CACTUS_SUPPORT
        ) {
            return false
        }
        return region.get(position.copy(y = position.y + 1)) !in LIQUID_BLOCK_STATES
    }

    private class PatchRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) : CactusFeatureRegion {
        private val externalWrites = HashMap<DecorationPos, Int>()

        override fun get(position: DecorationPos): Int =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else ->
                    externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
                        ?: Blocks.AIR.stateId
            }

        override fun set(
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val CACTUS_SUPPORT =
        setOf(
            "cactus",
            "sand",
            "red_sand",
            "suspicious_sand",
        )
    private val LAVA_STATE_IDS = Blocks.LAVA.stateId..Blocks.LAVA.stateId + 15
    private val LIQUID_BLOCK_STATES =
        (Blocks.WATER.stateId..Blocks.WATER.stateId + 15).toSet() +
            LAVA_STATE_IDS.toSet() +
            stateId("bubble_column")
    private val HORIZONTAL_OFFSETS = listOf(1 to 0, -1 to 0, 0 to 1, 0 to -1)
    private val SPECS =
        listOf(
            PatchSpec("patch_cactus_desert", 83, 6, setOf("desert")),
            PatchSpec("patch_cactus_decorated", 84, 13, setOf("badlands", "eroded_badlands", "wooded_badlands")),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
