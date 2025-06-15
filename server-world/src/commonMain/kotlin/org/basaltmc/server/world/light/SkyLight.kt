package org.basaltmc.server.world.light

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks

/**
 * Sky light for one chunk, column by column and then spread sideways.
 *
 * parity: `SkyLightEngine`. Light falls straight down a column at full strength until something
 * stops it, and from there it spreads in every direction losing one level per block, plus whatever
 * the block itself absorbs.
 *
 * Two deliberate limits, both visible only at a chunk border: the spread reads one block into the
 * neighbouring chunks rather than lighting the 3x3 the way vanilla does, and a neighbour that is
 * not loaded counts as open sky. Both err bright. The bug they replaced erred black over a whole
 * chunk, which is what a hill or an End pillar used to do to everything under it.
 */
public object SkyLight {
    public const val MAX: Int = 15

    /**
     * How much light a block takes out of what passes through it.
     *
     * parity: `BlockBehaviour.getLightDampening`, whose answer comes with the block's own shape:
     * fifteen for one that fills its cube, one for a shape that leaves a gap but holds a fluid,
     * zero for the rest. A slab or a trapdoor therefore stops nothing at all, which used to be the
     * known miss here back when everything solid-looking counted as opaque.
     */
    public fun opacity(stateId: Int): Int =
        if (stateId in 0 until TABLE.size) TABLE[stateId] else uncachedOpacity(stateId)

    private fun uncachedOpacity(stateId: Int): Int = BlockShapes.dampening(stateId)

    /**
     * The sky light of the middle chunk of [window].
     *
     * Only the band with terrain in it is lit: above the tallest block everything is open sky, and
     * more than fifteen blocks below the lowest surface no sky light can reach, since it loses a
     * level per block on the way down.
     */
    public fun compute(window: LightWindow): Array<IntArray> {
        val surface = window.surfaceRange()
        val topY = (surface.last + 1).coerceIn(window.minY, window.maxY)
        val bottomY = (surface.first - MAX - 1).coerceIn(window.minY, topY)
        val pass = LightSpread(window, bottomY, topY, outside = MAX)

        // parity: `ChunkSkyLightSources`: a column is a source at full strength down to the first
        // block that dampens light at all. Everything above that floor is simply 15.
        val floor = IntArray(LightWindow.WIDTH * LightWindow.WIDTH)
        for (z in 0 until LightWindow.WIDTH) {
            for (x in 0 until LightWindow.WIDTH) {
                var lowest = topY + 1
                for (y in topY downTo bottomY) {
                    if (pass.opacityAt(x, y, z) > 0) break
                    pass.set(x, y, z, MAX)
                    lowest = y
                }
                floor[z * LightWindow.WIDTH + x] = lowest
            }
        }

        // Only the edge of that lit region goes in the queue. A block of open sky with open sky all
        // round it has nothing to give anyone, and queueing it was most of the cost of a light pass.
        for (z in 0 until LightWindow.WIDTH) {
            for (x in 0 until LightWindow.WIDTH) {
                val lowest = floor[z * LightWindow.WIDTH + x]
                if (lowest > topY) continue
                var highestNeighbourFloor = lowest
                for (side in SIDES) {
                    val nextX = x + side[0]
                    val nextZ = z + side[1]
                    if (nextX !in 0 until LightWindow.WIDTH || nextZ !in 0 until LightWindow.WIDTH) continue
                    val neighbour = floor[nextZ * LightWindow.WIDTH + nextX]
                    if (neighbour > highestNeighbourFloor) highestNeighbourFloor = neighbour
                }
                for (y in lowest..minOf(highestNeighbourFloor, topY)) pass.push(x, y, z, MAX)
            }
        }
        return pass.run()
    }

    /**
     * One answer per block state, worked out once.
     *
     * The chain above costs a set lookup and a handful of calls, and a light pass asks it a few
     * hundred thousand times: as a table it measured two orders of magnitude cheaper, which is the
     * same reason the heightmaps have one.
     */
    private val SIDES =
        arrayOf(
            intArrayOf(1, 0),
            intArrayOf(-1, 0),
            intArrayOf(0, 1),
            intArrayOf(0, -1),
        )

    private val TABLE: IntArray = IntArray(Blocks.stateCount) { uncachedOpacity(it) }

    /** The same table, for the bulk fill that reads it a few hundred thousand times. */
    internal val opacityTable: IntArray get() = TABLE
}
