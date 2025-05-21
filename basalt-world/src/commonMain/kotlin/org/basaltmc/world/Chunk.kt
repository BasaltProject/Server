package org.basaltmc.world

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier

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

    /**
     * Topmost non-air, motion-blocking Y per column (16x16, x-major then z, matching vanilla's
     * `MOTION_BLOCKING` heightmap layout). Real vanilla derives "motion blocking" from each
     * block's own collision shape; this project has no collision model yet, so only the specific
     * non-colliding blocks vegetal decoration actually places (grass, flowers, bush) are excluded
     * here, not a general rule.
     */
    public fun heightmap(): IntArray =
        IntArray(16 * 16) { i ->
            val x = i and 15
            val z = i shr 4
            var y = WorldHeight.MAX_Y
            while (y >= WorldHeight.MIN_Y && isTransparentForHeightmap(getBlockStateId(x, y, z))) y--
            y + 1
        }

    private fun isTransparentForHeightmap(stateId: Int): Boolean =
        stateId == Blocks.AIR.stateId || stateId in NON_COLLIDING_STATE_IDS

    private companion object {
        val NON_COLLIDING_STATE_IDS: Set<Int> =
            listOf(
                "short_grass",
                "tall_grass",
                "bush",
                "dandelion",
                "poppy",
                "azure_bluet",
                "oxeye_daisy",
                "cornflower",
                "orange_tulip",
                "red_tulip",
                "pink_tulip",
                "white_tulip",
            ).mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()
    }
}
