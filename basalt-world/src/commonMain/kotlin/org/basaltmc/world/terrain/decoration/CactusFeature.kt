package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.feature.BlockColumnFeature, narrowed to real
 * `cactus`'s own single-layer real config: `BiasedToBottomInt.of(1, 3)` cactus blocks stacked from
 * [DecorationPos]'s own Y upward. The `cactus_flower` second layer (a `1/4` chance real vanilla adds
 * on top) is not ported: a newer addition with its own block this project doesn't model yet.
 */
public object CactusFeature {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        if (origin.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        val localX = origin.x and 15
        val localZ = origin.z and 15
        if (chunk.getBlockStateId(localX, origin.y, localZ) != Blocks.AIR.stateId) return false
        if (origin.y - 1 < WorldHeight.MIN_Y || chunk.getBlockStateId(localX, origin.y - 1, localZ) == Blocks.AIR.stateId) {
            return false
        }
        // parity: BiasedToBottomInt.sample: min + nextInt(nextInt(max - min + 1) + 1).
        val height = 1 + random.nextInt(random.nextInt(3) + 1)
        for (i in 0 until height) {
            val y = origin.y + i
            if (y > WorldHeight.MAX_Y || chunk.getBlockStateId(localX, y, localZ) != Blocks.AIR.stateId) break
            chunk.setBlockStateId(localX, y, localZ, cactusStateId)
        }
        return true
    }

    private val cactusStateId =
        Blocks.REGISTRY.byIdentifier(org.basaltmc.registry.Identifier.vanilla("cactus"))?.stateId ?: error("unknown block 'cactus'")
}
