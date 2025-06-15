package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk

/**
 * The part both light engines share: read the chunk and one block of its neighbours, seed it, and
 * let the light run out one level per block plus whatever each block absorbs.
 *
 * parity: `LayerLightEngine`. What changes between sky and block light is only where the light
 * starts, which is why the seeding is the caller's.
 */
internal object LightSpread {
    const val SKIRT_WIDTH = 18
    const val BLOCKS_PER_SECTION = 16 * 16 * 16

    val SIDES =
        arrayOf(
            intArrayOf(1, 0, 0),
            intArrayOf(-1, 0, 0),
            intArrayOf(0, 1, 0),
            intArrayOf(0, -1, 0),
            intArrayOf(0, 0, 1),
            intArrayOf(0, 0, -1),
        )

    fun index(
        x: Int,
        y: Int,
        z: Int,
    ): Int = (y * SKIRT_WIDTH + z) * SKIRT_WIDTH + x

    /** Light from every emitting block, spread out. Used by [BlockLight]. */
    fun compute(
        chunk: Chunk,
        blockAt: (x: Int, y: Int, z: Int) -> Int?,
        emission: (Int) -> Int,
    ): Array<IntArray> {
        val minY = chunk.verticalBounds.minY
        val height = chunk.verticalBounds.height
        val stride = SKIRT_WIDTH * SKIRT_WIDTH
        val light = ByteArray(stride * height)
        val opacity = ByteArray(stride * height)
        val queue = ArrayDeque<Int>()

        for (localZ in 0 until SKIRT_WIDTH) {
            for (localX in 0 until SKIRT_WIDTH) {
                val worldX = chunk.chunkX * 16 + localX - 1
                val worldZ = chunk.chunkZ * 16 + localZ - 1
                val inside = localX in 1..16 && localZ in 1..16
                for (y in 0 until height) {
                    val state =
                        if (inside) {
                            chunk.getBlockStateId(localX - 1, minY + y, localZ - 1)
                        } else {
                            blockAt(worldX, minY + y, worldZ) ?: Blocks.AIR.stateId
                        }
                    val at = index(localX, y, localZ)
                    opacity[at] = SkyLight.opacity(state).toByte()
                    val emitted = emission(state)
                    if (emitted > 0) {
                        light[at] = emitted.toByte()
                        queue.addLast(at)
                    }
                }
            }
        }

        spread(light, opacity, queue, height)
        return sections(chunk, light, height)
    }

    fun spread(
        light: ByteArray,
        opacity: ByteArray,
        queue: ArrayDeque<Int>,
        height: Int,
    ) {
        while (queue.isNotEmpty()) {
            val at = queue.removeFirst()
            val level = light[at].toInt()
            if (level <= 1) continue
            val x = at % SKIRT_WIDTH
            val z = (at / SKIRT_WIDTH) % SKIRT_WIDTH
            val y = at / (SKIRT_WIDTH * SKIRT_WIDTH)
            for (side in SIDES) {
                val nextX = x + side[0]
                val nextY = y + side[1]
                val nextZ = z + side[2]
                if (nextX !in 0 until SKIRT_WIDTH || nextZ !in 0 until SKIRT_WIDTH) continue
                if (nextY !in 0 until height) continue
                val target = index(nextX, nextY, nextZ)
                val next = level - 1 - opacity[target].toInt()
                if (next <= light[target].toInt()) continue
                light[target] = next.toByte()
                queue.addLast(target)
            }
        }
    }

    /** The inner 16x16 of the working grid, one array per section. */
    fun sections(
        chunk: Chunk,
        light: ByteArray,
        height: Int,
        aboveTop: Int = 0,
        top: Int = height - 1,
    ): Array<IntArray> =
        Array(chunk.sectionCount) { section ->
            val values = IntArray(BLOCKS_PER_SECTION)
            for (y in 0 until 16) {
                val worldY = section * 16 + y
                if (worldY >= height) break
                val open = worldY > top
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        values[(y * 16 + z) * 16 + x] =
                            if (open) aboveTop else light[index(x + 1, worldY, z + 1)].toInt()
                    }
                }
            }
            values
        }
}
