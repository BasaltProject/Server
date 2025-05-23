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
     * Sets the biome network ID at quart (4-block) resolution. [quartY] is the world quart
     * (0 until [WorldHeight.HEIGHT] / 4), [quartX]/[quartZ] chunk-local (0..3).
     */
    public fun setBiomeId(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        biomeId: Int,
    ) {
        require(quartY in 0 until WorldHeight.HEIGHT / 4) { "quartY=$quartY out of world bounds" }
        sections[quartY shr 2].setBiomeId(quartX, quartY and 3, quartZ, biomeId)
    }

    /** Topmost matching Y plus one per column, matching vanilla's heightmap convention. */
    public fun heightmap(type: HeightmapType = HeightmapType.MOTION_BLOCKING): IntArray =
        IntArray(16 * 16) { i ->
            val x = i and 15
            val z = i shr 4
            heightAt(x, z, type)
        }

    /** The heightmap value for one local column, without allocating/scanning the other 255. */
    public fun heightAt(
        x: Int,
        z: Int,
        type: HeightmapType = HeightmapType.MOTION_BLOCKING,
    ): Int {
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && !type.matches(getBlockStateId(x, y, z))) y--
        return y + 1
    }

    public enum class HeightmapType {
        /** // parity: Heightmap.Types.WORLD_SURFACE(_WG), every non-air block and fluid. */
        WORLD_SURFACE,

        /** // parity: Heightmap.Types.OCEAN_FLOOR(_WG), blocks that stop motion but not fluids. */
        OCEAN_FLOOR,

        /** // parity: Heightmap.Types.MOTION_BLOCKING, motion-blocking blocks plus fluids. */
        MOTION_BLOCKING,
        ;

        internal fun matches(stateId: Int): Boolean =
            when (this) {
                WORLD_SURFACE -> stateId != Blocks.AIR.stateId
                OCEAN_FLOOR ->
                    stateId != Blocks.AIR.stateId &&
                        stateId != Blocks.WATER.stateId &&
                        stateId !in NON_COLLIDING_STATE_IDS
                MOTION_BLOCKING ->
                    Blocks.hasWaterFluid(stateId) ||
                        (stateId != Blocks.AIR.stateId && stateId !in NON_COLLIDING_STATE_IDS)
            }

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
                    "seagrass",
                    "tall_seagrass",
                    "kelp",
                    "kelp_plant",
                    "sea_pickle",
                    "tube_coral",
                    "brain_coral",
                    "bubble_coral",
                    "fire_coral",
                    "horn_coral",
                    "tube_coral_fan",
                    "brain_coral_fan",
                    "bubble_coral_fan",
                    "fire_coral_fan",
                    "horn_coral_fan",
                    "tube_coral_wall_fan",
                    "brain_coral_wall_fan",
                    "bubble_coral_wall_fan",
                    "fire_coral_wall_fan",
                    "horn_coral_wall_fan",
                ).mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()
        }
    }
}
