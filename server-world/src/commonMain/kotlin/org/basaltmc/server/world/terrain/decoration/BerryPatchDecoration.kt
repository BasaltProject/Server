package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the common and rare sweet-berry patches. */
public object BerryPatchDecoration {
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
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) continue
            val origin = DecorationPos(x, y, z)
            repeat(96) {
                val target =
                    DecorationPos(
                        origin.x + random.nextInt(8) - random.nextInt(8),
                        origin.y + random.nextInt(4) - random.nextInt(4),
                        origin.z + random.nextInt(8) - random.nextInt(8),
                    )
                if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y ||
                    !Blocks.isAir(region.get(target))
                ) {
                    return@repeat
                }
                if (target.y == WorldHeight.MIN_Y ||
                    region.get(target.copy(y = target.y - 1)) != Blocks.GRASS_BLOCK.stateId
                ) {
                    return@repeat
                }
                region.set(target, SWEET_BERRY_AGE_3_STATE_ID)
            }
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
                externalWrites[position] = stateId
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

    private val SWEET_BERRY_AGE_3_STATE_ID =
        (
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sweet_berry_bush"))?.stateId
                ?: error("unknown block 'sweet_berry_bush'")
        ) + 3
    private val SPECS =
        listOf(
            PatchSpec(
                "patch_berry_common",
                94,
                32,
                setOf("old_growth_pine_taiga", "old_growth_spruce_taiga", "taiga"),
            ),
            PatchSpec("patch_berry_rare", 95, 384, setOf("snowy_taiga")),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
