package org.basaltmc.world

import org.basaltmc.registry.Blocks

/** One 16x16x16 slice of a [Chunk]. Block state IDs default to 0, which is [Blocks.AIR]'s ID. */
public class ChunkSection {
    private val blockStateIds = IntArray(16 * 16 * 16)

    public fun getBlockStateId(
        x: Int,
        y: Int,
        z: Int,
    ): Int = blockStateIds[index(x, y, z)]

    public fun setBlockStateId(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        blockStateIds[index(x, y, z)] = stateId
    }

    /** Blocks that count toward lighting (i.e. not air), matching vanilla's per-section block count. */
    public val nonAirBlockCount: Int
        get() = blockStateIds.count { it != Blocks.AIR.stateId }

    /** A copy of the raw state IDs, in `(y * 16 + z) * 16 + x` order: the layout `PalettedContainer` expects. */
    public fun blockStateIds(): IntArray = blockStateIds.copyOf()

    /** Overwrites every state ID at once, in the same `(y * 16 + z) * 16 + x` order [blockStateIds] returns. */
    public fun setBlockStateIds(ids: IntArray) {
        require(ids.size == blockStateIds.size) { "expected ${blockStateIds.size} state ids, got ${ids.size}" }
        ids.copyInto(blockStateIds)
    }

    private fun index(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        require(x in 0..15 && y in 0..15 && z in 0..15) { "coordinates out of section bounds: ($x, $y, $z)" }
        return (y shl 8) or (z shl 4) or x
    }
}
