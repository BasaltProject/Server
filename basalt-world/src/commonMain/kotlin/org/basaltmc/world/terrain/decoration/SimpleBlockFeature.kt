package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.feature.SimpleBlockFeature. Places a single block
 * at [origin] if it "can survive" there. Real vanilla's `BlockState.canSurvive` dispatches to each
 * block's own placement rule; only the shape every Plains vegetal-decoration feature needs is
 * ported: the target is air, the block below is solid (a flower/grass/bush needs ground to stand
 * on), and for [isDoublePlant] blocks (`tall_grass`), the block above is also air (matching real
 * `SimpleBlockFeature`'s own `DoublePlantBlock` special case, which places both halves at once).
 */
public class SimpleBlockFeature(
    private val toPlace: (random: WorldgenRandom, origin: DecorationPos) -> Int,
    private val isDoublePlant: Boolean = false,
) {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        // parity: vanilla asks the state provider before evaluating canSurvive, so weighted/noise
        // providers consume RNG even when the candidate ultimately cannot be placed.
        val stateId = toPlace(random, origin)
        if (origin.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        val localX = origin.x and 15
        val localZ = origin.z and 15
        if (!Blocks.isAir(chunk.getBlockStateId(localX, origin.y, localZ))) return false
        if (origin.y - 1 < WorldHeight.MIN_Y ||
            !Chunk.HeightmapType.OCEAN_FLOOR.matches(chunk.getBlockStateId(localX, origin.y - 1, localZ))
        ) {
            return false
        }
        if (isDoublePlant) {
            if (origin.y + 1 > WorldHeight.MAX_Y ||
                !Blocks.isAir(chunk.getBlockStateId(localX, origin.y + 1, localZ))
            ) {
                return false
            }
        }

        chunk.setBlockStateId(localX, origin.y, localZ, stateId)
        if (isDoublePlant) {
            val upperStateId =
                Blocks.upperDoublePlantStateId(stateId) ?: error("state $stateId is not a lower double plant")
            chunk.setBlockStateId(localX, origin.y + 1, localZ, upperStateId)
        }
        return true
    }
}
