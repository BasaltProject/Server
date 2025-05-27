package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

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
        val localX = origin.x and 15
        val localZ = origin.z and 15
        if (chunk.getBlockStateId(localX, origin.y, localZ) != Blocks.AIR.stateId) return false
        if (origin.y - 1 < WorldHeight.MIN_Y ||
            chunk.getBlockStateId(localX, origin.y - 1, localZ) == Blocks.AIR.stateId
        ) {
            return false
        }
        // parity: BiasedToBottomInt.sample: min + nextInt(nextInt(max - min + 1) + 1); the flower
        // layer's WeightedListInt draws nextInt(4) with only the last slot blooming.
        val cactusHeight = 1 + random.nextInt(random.nextInt(3) + 1)
        val flowerHeight = if (random.nextInt(4) == 3) 1 else 0
        val totalHeight = cactusHeight + flowerHeight

        // parity: the real allowed-placement walk checks air at origin+1 .. origin+totalHeight and
        // truncates the flower layer first, then the cactus body.
        var newHeight = totalHeight
        for (dy in 1..totalHeight) {
            val y = origin.y + dy
            if (y > WorldHeight.MAX_Y || chunk.getBlockStateId(localX, y, localZ) != Blocks.AIR.stateId) {
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
            chunk.setBlockStateId(localX, y, localZ, cactusStateId)
            y++
        }
        repeat(flowers) {
            chunk.setBlockStateId(localX, y, localZ, cactusFlowerStateId)
            y++
        }
        return true
    }

    private val cactusStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.registry.Identifier
                    .vanilla("cactus"),
            )?.stateId
            ?: error("unknown block 'cactus'")
    private val cactusFlowerStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.registry.Identifier
                    .vanilla("cactus_flower"),
            )?.stateId
            ?: error("unknown block 'cactus_flower'")
}
