package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.light.BlockLight
import org.basaltmc.server.world.light.LightWindow
import org.basaltmc.server.world.light.SkyLight
import org.basaltmc.wire.protocol.play.NibbleArray

/**
 * The light level at a position, from the same pass that fills the light packet.
 *
 * The engine works a chunk at a time and keeps its answer on the chunk, so asking here costs a
 * lookup once that pass has run and a pass when it has not.
 */
public object WorldLight {
    public const val MAX_LEVEL: Int = 15

    /** parity: `Level.getBrightness(LightLayer.BLOCK, pos)`, what ice and snow ask before melting. */
    public fun blockLight(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = level(world, x, y, z, sky = false)

    /**
     * parity: `Level.getRawBrightness(pos, 0)`, the brighter of the two layers.
     *
     * The zero matters: crops go by the sky light that reaches them, not by the time of day, which
     * is why a field keeps growing at night.
     */
    public fun rawBrightness(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = maxOf(level(world, x, y, z, sky = false), level(world, x, y, z, sky = true))

    private fun level(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        sky: Boolean,
    ): Int {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return 0
        if (y < chunk.verticalBounds.minY || y > chunk.verticalBounds.maxY) return 0
        if (sky && !world.declaration.dimension.hasSkylight) return 0
        val window = LightWindow(chunk) { chunkX, chunkZ -> world.state.chunkIfLoaded(chunkX, chunkZ) }
        val sections =
            if (sky) {
                chunk.skyLight(keep = window.isComplete) {
                    val computed = SkyLight.compute(window)
                    Array(computed.size) { NibbleArray.pack(computed[it]) }
                }
            } else {
                chunk.blockLight(keep = window.isComplete) {
                    val computed = BlockLight.compute(window) ?: return@blockLight emptyArray()
                    Array(computed.size) { NibbleArray.pack(computed[it]) }
                }
            }
        val section = (y - chunk.verticalBounds.minY) shr 4
        if (section !in sections.indices) return 0
        val bytes = sections[section]
        if (bytes.isEmpty()) return 0
        val index = ((y and 15) shl 8) or ((z and 15) shl 4) or (x and 15)
        val byte = bytes[index / 2].toInt()
        return if (index % 2 == 0) byte and 0xF else (byte shr 4) and 0xF
    }
}
