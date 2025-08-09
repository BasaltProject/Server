package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.feature.BlockColumnFeature, narrowed to real
 * `cactus`'s own two-layer real config: a `BiasedToBottomInt.of(1, 3)` cactus column, then a
 * `cactus_flower` tip layer whose real `weighted_list` height is 0 with weight 3 and 1 with
 * weight 1. Both heights are sampled before anything is placed (the real jar samples every layer
 * up front), the column truncates from the tip down on the first non-air position above the
 * origin (`prioritize_tip: false`), and placement itself overwrites, exactly like the real
 * unconditional `setBlock`.
 */
public object CactusFeature {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        if (origin.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        val region = ChunkCactusRegion(chunk)
        if (!canPlace(region, origin)) return false
        return placeColumn(region, random, origin)
    }

    internal fun canPlace(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean = canPlace(ChunkCactusRegion(chunk), origin)

    /** // parity: CactusBlock.canSurvive, used by the placed feature's would_survive filter. */
    private fun canPlace(
        region: CactusFeatureRegion,
        origin: DecorationPos,
    ): Boolean {
        if (origin.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(origin))) return false
        for ((dx, dz) in HORIZONTAL_OFFSETS) {
            val neighbour = region.get(origin.copy(x = origin.x + dx, z = origin.z + dz))
            if (BlockShapes.isSolidState(neighbour) || neighbour in LAVA_STATE_IDS) return false
        }
        if (origin.y == WorldHeight.MIN_Y ||
            Blocks.byStateId(region.get(origin.copy(y = origin.y - 1)))?.block?.path !in CACTUS_SUPPORT
        ) {
            return false
        }
        return !Blocks.hasFluid(region.get(origin.copy(y = origin.y + 1)))
    }

    internal fun placeColumn(
        region: CactusFeatureRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        // parity: BiasedToBottomInt.sample: min + nextInt(nextInt(max - min + 1) + 1); the flower
        // layer's WeightedListInt draws nextInt(4) with only the last slot blooming.
        val cactusHeight = 1 + random.nextInt(random.nextInt(3) + 1)
        val flowerHeight = if (random.nextInt(4) == 3) 1 else 0
        val totalHeight = cactusHeight + flowerHeight

        // parity: the real allowed-placement walk checks air at origin+1 .. origin+totalHeight and
        // truncates the flower layer first, then the cactus body.
        var newHeight = totalHeight
        for (dy in 1..totalHeight) {
            val position = origin.copy(y = origin.y + dy)
            if (position.y > WorldHeight.MAX_Y || !Blocks.isAir(region.get(position))) {
                newHeight = dy - 1
                break
            }
        }
        var flowers = flowerHeight
        var cacti = cactusHeight
        var toRemove = totalHeight - newHeight
        val trimmedFlowers = minOf(flowers, toRemove)
        flowers -= trimmedFlowers
        toRemove -= trimmedFlowers
        cacti -= minOf(cacti, toRemove)

        var y = origin.y
        repeat(cacti) {
            region.set(origin.copy(y = y), cactusStateId)
            y++
        }
        repeat(flowers) {
            region.set(origin.copy(y = y), cactusFlowerStateId)
            y++
        }
        return true
    }

    private class ChunkCactusRegion(
        private val chunk: Chunk,
    ) : CactusFeatureRegion {
        override fun get(position: DecorationPos): Int =
            if (position.y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                chunk.getDecorationBlockStateId(position.x, position.y, position.z)
            } else {
                Blocks.AIR.stateId
            }

        override fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId)
            }
        }
    }

    private val CACTUS_SUPPORT = setOf("cactus", "sand", "red_sand", "suspicious_sand")
    private val LAVA_STATE_IDS = Blocks.LAVA.stateId..Blocks.LAVA.stateId + 15
    private val HORIZONTAL_OFFSETS = listOf(1 to 0, -1 to 0, 0 to 1, 0 to -1)

    private val cactusStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("cactus"),
            )?.stateId
            ?: error("unknown block 'cactus'")
    private val cactusFlowerStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("cactus_flower"),
            )?.stateId
            ?: error("unknown block 'cactus_flower'")
}

internal interface CactusFeatureRegion {
    fun get(position: DecorationPos): Int

    fun set(
        position: DecorationPos,
        stateId: Int,
    )
}
