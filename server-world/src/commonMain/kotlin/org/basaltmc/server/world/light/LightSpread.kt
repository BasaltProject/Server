package org.basaltmc.server.world.light

import org.basaltmc.server.world.Chunk

/**
 * The part both light engines share: a working grid over one [LightWindow], the flood fill, and
 * cutting the middle chunk back out of it.
 *
 * parity: `LayerLightEngine`. What changes between sky and block light is only where the light
 * starts, which is why the seeding is the caller's.
 */
internal class LightSpread(
    private val window: LightWindow,
    /** The band of the world this pass covers. Everything outside it is left at [outside]. */
    val bottomY: Int,
    val topY: Int,
    private val outside: Int,
) {
    private val height = topY - bottomY + 1
    private val light = ByteArray(LightWindow.WIDTH * LightWindow.WIDTH * height)
    private val opacity = ByteArray(LightWindow.WIDTH * LightWindow.WIDTH * height)
    private var queue = IntArray(INITIAL_QUEUE)
    private var queueHead = 0
    private var queueTail = 0

    init {
        window.fillOpacity(opacity, bottomY, topY, SkyLight.opacityTable)
    }

    fun stateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = window.stateAt(x, y, z)

    fun opacityAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = opacity[index(x, y, z)].toInt()

    /** Writes a level without queueing it: for light that has nowhere new to go. */
    fun set(
        x: Int,
        y: Int,
        z: Int,
        level: Int,
    ) {
        light[index(x, y, z)] = level.toByte()
    }

    /** Writes a level and queues it even if it is already there, which [seed] deliberately will not. */
    fun push(
        x: Int,
        y: Int,
        z: Int,
        level: Int,
    ) {
        val at = index(x, y, z)
        light[at] = level.toByte()
        enqueue(at)
    }

    fun seed(
        x: Int,
        y: Int,
        z: Int,
        level: Int,
    ) {
        val at = index(x, y, z)
        if (level <= light[at].toInt()) return
        light[at] = level.toByte()
        enqueue(at)
    }

    /** A growable int queue: `ArrayDeque<Int>` boxes every node, and there are hundreds of thousands. */
    private fun enqueue(at: Int) {
        if (queueTail == queue.size) queue = queue.copyOf(queue.size * 2)
        queue[queueTail++] = at
    }

    /** Runs the fill and answers the middle chunk's light, one array per section. */
    fun run(): Array<IntArray> {
        val width = LightWindow.WIDTH
        val layer = width * width
        val cells = light.size
        while (queueHead < queueTail) {
            val at = queue[queueHead++]
            val level = light[at].toInt()
            if (level <= 1) continue
            val x = at % width
            val z = (at / width) % width
            // Straight index arithmetic rather than a table of offsets: this is the innermost loop
            // of the whole engine and it runs a few hundred thousand times per chunk.
            if (x > 0) push(at - 1, level)
            if (x < width - 1) push(at + 1, level)
            if (z > 0) push(at - width, level)
            if (z < width - 1) push(at + width, level)
            if (at >= layer) push(at - layer, level)
            if (at + layer < cells) push(at + layer, level)
        }
        return centreSections()
    }

    /** parity: `LightEngine.getOpacity`, max(1, dampening): air costs a level, and so does water. */
    private fun push(
        target: Int,
        fromLevel: Int,
    ) {
        val next = fromLevel - maxOf(1, opacity[target].toInt())
        if (next <= light[target].toInt()) return
        light[target] = next.toByte()
        enqueue(target)
    }

    private fun centreSections(): Array<IntArray> {
        val chunk: Chunk = window.centre
        return Array(chunk.sectionCount) { section ->
            val values = IntArray(BLOCKS_PER_SECTION)
            val sectionMinY = chunk.verticalBounds.minY + section * 16
            for (y in 0 until 16) {
                val worldY = sectionMinY + y
                val inside = worldY in bottomY..topY
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        values[(y * 16 + z) * 16 + x] =
                            if (inside) light[index(x + 16, worldY, z + 16)].toInt() else outside
                    }
                }
            }
            values
        }
    }

    private fun index(
        x: Int,
        y: Int,
        z: Int,
    ): Int = ((y - bottomY) * LightWindow.WIDTH + z) * LightWindow.WIDTH + x

    companion object {
        const val BLOCKS_PER_SECTION: Int = 16 * 16 * 16
        private const val INITIAL_QUEUE = 1 shl 16
    }
}
