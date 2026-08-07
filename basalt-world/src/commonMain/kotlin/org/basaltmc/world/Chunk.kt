package org.basaltmc.world

import org.basaltmc.registry.Blocks

/** A 16-wide, [WorldHeight.HEIGHT]-tall column of blocks at ([chunkX], [chunkZ]) in chunk coordinates. */
public class Chunk(
    public val chunkX: Int,
    public val chunkZ: Int,
) {
    private val sections = Array(WorldHeight.SECTION_COUNT) { ChunkSection() }

    public val sectionCount: Int get() = sections.size

    public fun section(index: Int): ChunkSection = sections[index]

    public fun getBlockStateId(
        x: Int,
        y: Int,
        z: Int,
    ): Int = sectionFor(y).getBlockStateId(x, y and 15, z)

    public fun setBlockStateId(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        sectionFor(y).setBlockStateId(x, y and 15, z, stateId)
    }

    private fun sectionFor(y: Int): ChunkSection {
        require(y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) { "y=$y out of world bounds" }
        return sections[(y shr 4) - WorldHeight.MIN_SECTION]
    }

    /** Topmost non-air Y per column (16x16, x-major then z, matching vanilla's `MOTION_BLOCKING` layout). */
    public fun heightmap(): IntArray =
        IntArray(16 * 16) { i ->
            val x = i and 15
            val z = i shr 4
            var y = WorldHeight.MAX_Y
            while (y >= WorldHeight.MIN_Y && getBlockStateId(x, y, z) == Blocks.AIR.stateId) y--
            y + 1
        }
}
