package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight

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
        if (!Blocks.isAir(chunk.getDecorationBlockStateId(origin.x, origin.y, origin.z))) return false
        if (origin.y - 1 < WorldHeight.MIN_Y ||
            !Chunk.HeightmapType.OCEAN_FLOOR.matches(
                chunk.getDecorationBlockStateId(origin.x, origin.y - 1, origin.z),
            )
        ) {
            return false
        }
        if (isDoublePlant) {
            if (origin.y + 1 > WorldHeight.MAX_Y ||
                !Blocks.isAir(chunk.getDecorationBlockStateId(origin.x, origin.y + 1, origin.z))
            ) {
                return false
            }
        }

        chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, stateId)
        if (isDoublePlant) {
            val upperStateId =
                Blocks.upperDoublePlantStateId(stateId) ?: error("state $stateId is not a lower double plant")
            chunk.setDecorationBlockStateId(origin.x, origin.y + 1, origin.z, upperStateId)
        }
        return true
    }
}
