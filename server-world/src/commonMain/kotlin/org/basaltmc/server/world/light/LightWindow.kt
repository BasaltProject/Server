package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk

/**
 * The chunk being lit and the eight around it, read straight off the chunk objects.
 *
 * Three by three is not a round number, it is the reach of light: fifteen levels is less than a
 * chunk, so nothing outside this window can reach the middle one. It is also why the lookups take
 * no lock: a light pass reads a few hundred thousand blocks, and going through the world's own
 * accessor for each of them cost more than the lighting.
 *
 * A neighbour that is not loaded reads as air, so light pours in from where nothing is known yet.
 * That errs bright, which is the side to err on: the black band this replaced was a chunk wide.
 */
public class LightWindow(
    public val centre: Chunk,
    private val neighbours: (chunkX: Int, chunkZ: Int) -> Chunk?,
) {
    public val minY: Int = centre.verticalBounds.minY
    public val maxY: Int = centre.verticalBounds.maxY

    private val originX = (centre.chunkX - 1) * 16
    private val originZ = (centre.chunkZ - 1) * 16

    /**
     * The nine chunks, looked up once.
     *
     * Per block it was a lookup a few hundred thousand times over, which is the sort of thing that
     * turns a light pass into a stall.
     */
    private val chunks: Array<Chunk?> =
        Array(9) { index ->
            val offsetX = index % 3 - 1
            val offsetZ = index / 3 - 1
            if (offsetX == 0 && offsetZ == 0) {
                centre
            } else {
                neighbours(centre.chunkX + offsetX, centre.chunkZ + offsetZ)
            }
        }

    private fun chunkAt(
        offsetX: Int,
        offsetZ: Int,
    ): Chunk? = chunks[(offsetZ + 1) * 3 + offsetX + 1]

    /**
     * Copies the opacity of a whole band into [into], one byte per block of the window.
     *
     * Section by section rather than block by block: unpacking a paletted container in one go is
     * what the chunk codec already does to send one, and it measured an order of magnitude cheaper
     * than asking it for four thousand blocks one at a time.
     */
    public fun fillOpacity(
        into: ByteArray,
        bottomY: Int,
        topY: Int,
        opacityOf: IntArray,
    ) {
        val scratch = IntArray(BLOCKS_PER_SECTION)
        for (offsetZ in -1..1) {
            for (offsetX in -1..1) {
                val chunk = chunkAt(offsetX, offsetZ) ?: continue
                val baseX = (offsetX + 1) * 16
                val baseZ = (offsetZ + 1) * 16
                for (index in 0 until chunk.sectionCount) {
                    val sectionMinY = minY + index * 16
                    if (sectionMinY > topY || sectionMinY + 15 < bottomY) continue
                    val section = chunk.section(index)
                    // Air takes nothing out of the light, and the array starts at zero.
                    if (section.nonAirBlockCount == 0) continue
                    // Solid rock is most of a column and every block of it answers the same, so it
                    // is written straight in rather than unpacked block by block.
                    val solid = !section.mayContain { opacityOf[it] < SOLID }
                    if (!solid) section.copyBlockStateIdsInto(scratch)
                    for (localY in 0 until 16) {
                        val y = sectionMinY + localY
                        if (y < bottomY || y > topY) continue
                        val row = (y - bottomY) * WIDTH * WIDTH
                        for (localZ in 0 until 16) {
                            val target = row + (baseZ + localZ) * WIDTH + baseX
                            val source = (localY * 16 + localZ) * 16
                            if (solid) {
                                into.fill(SOLID.toByte(), target, target + 16)
                            } else {
                                for (localX in 0 until 16) {
                                    into[target + localX] = opacityOf[scratch[source + localX]].toByte()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** The block at a window coordinate, where 0 is one chunk west and north of the middle one. */
    public fun stateAt(
        windowX: Int,
        y: Int,
        windowZ: Int,
    ): Int {
        if (y < minY || y > maxY) return Blocks.AIR.stateId
        val worldX = originX + windowX
        val worldZ = originZ + windowZ
        val chunk = chunkAt(windowX / 16 - 1, windowZ / 16 - 1) ?: return Blocks.AIR.stateId
        return chunk.getBlockStateId(worldX and 15, y, worldZ and 15)
    }

    /** The tallest and shortest surface in the window, which is the band worth lighting. */
    public fun surfaceRange(): IntRange {
        var lowest = Int.MAX_VALUE
        var highest = Int.MIN_VALUE
        for (chunk in chunks) {
            if (chunk != null) {
                val heights = chunk.heightmap(Chunk.HeightmapType.WORLD_SURFACE)
                for (height in heights) {
                    if (height < lowest) lowest = height
                    if (height > highest) highest = height
                }
            }
        }
        if (highest == Int.MIN_VALUE) return minY..minY
        return lowest..highest
    }

    /** The sections any of the nine chunks lights up, so an unlit band costs nothing. */
    public fun emitterSections(emission: (Int) -> Int): IntRange? {
        var lowest = Int.MAX_VALUE
        var highest = Int.MIN_VALUE
        for (chunk in chunks) {
            if (chunk != null) {
                for (index in 0 until chunk.sectionCount) {
                    if (!chunk.section(index).mayContain { emission(it) > 0 }) continue
                    if (index < lowest) lowest = index
                    if (index > highest) highest = index
                }
            }
        }
        if (highest == Int.MIN_VALUE) return null
        return lowest..highest
    }

    public companion object {
        public const val WIDTH: Int = 48
        private const val BLOCKS_PER_SECTION = 16 * 16 * 16
        private const val SOLID = 15
    }
}
