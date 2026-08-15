package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

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

    /** How much light a block takes out of what passes through it. parity: `BlockBehaviour.lightBlock`. */
    public fun opacity(stateId: Int): Int =
        when {
            Blocks.isAir(stateId) -> 0
            stateId in TRANSPARENT -> 0
            Blocks.isLeavesState(stateId) -> 1
            Blocks.hasFluid(stateId) -> 1
            // Anything that does not stop a falling player does not stop light either: grass,
            // flowers, torches, rails.
            !Chunk.HeightmapType.MOTION_BLOCKING.matches(stateId) -> 0
            else -> MAX
        }

    /**
     * The light of every block in [chunk], as one value per block in section order.
     *
     * [blockAt] reads world coordinates outside the chunk and answers null where nothing is loaded,
     * which is read as open sky.
     */
    public fun compute(
        chunk: Chunk,
        blockAt: (x: Int, y: Int, z: Int) -> Int?,
    ): Array<IntArray> {
        val minY = chunk.verticalBounds.minY
        val height = chunk.verticalBounds.height
        // Nothing above the tallest block casts or takes a shadow, so the whole engine runs on the
        // band that has terrain in it. Over a normal world that is a third of the column.
        val top = (chunk.heightmap(Chunk.HeightmapType.WORLD_SURFACE).max() - minY + 1).coerceIn(0, height - 1)
        val stride = LightSpread.SKIRT_WIDTH * LightSpread.SKIRT_WIDTH
        val light = ByteArray(stride * (top + 1))
        val opacity = ByteArray(stride * (top + 1))

        // The chunk plus one block of its neighbours, so light spilling in from an open column next
        // door still arrives.
        for (localZ in 0 until LightSpread.SKIRT_WIDTH) {
            for (localX in 0 until LightSpread.SKIRT_WIDTH) {
                val worldX = chunk.chunkX * 16 + localX - 1
                val worldZ = chunk.chunkZ * 16 + localZ - 1
                val inside = localX in 1..16 && localZ in 1..16
                for (y in 0..top) {
                    val state =
                        if (inside) {
                            chunk.getBlockStateId(localX - 1, minY + y, localZ - 1)
                        } else {
                            blockAt(worldX, minY + y, worldZ) ?: Blocks.AIR.stateId
                        }
                    opacity[LightSpread.index(localX, y, localZ)] = opacity(state).toByte()
                }
            }
        }

        // parity: the sky pours down a column at full strength until a block takes some of it.
        val queue = ArrayDeque<Int>()
        for (localZ in 0 until LightSpread.SKIRT_WIDTH) {
            for (localX in 0 until LightSpread.SKIRT_WIDTH) {
                var level = MAX
                for (y in top downTo 0) {
                    val at = LightSpread.index(localX, y, localZ)
                    level -= opacity[at].toInt()
                    if (level <= 0) break
                    light[at] = level.toByte()
                    queue.addLast(at)
                }
            }
        }

        LightSpread.spread(light, opacity, queue, top + 1)
        return LightSpread.sections(chunk, light, height, aboveTop = MAX, top = top)
    }

    private val DYE_COLOURS =
        listOf(
            "white",
            "orange",
            "magenta",
            "light_blue",
            "yellow",
            "lime",
            "pink",
            "gray",
            "light_gray",
            "cyan",
            "purple",
            "blue",
            "brown",
            "green",
            "red",
            "black",
        )

    /** The blocks light goes through untouched, which vanilla decides from their render shape. */
    private val TRANSPARENT: Set<Int> =
        buildSet {
            for (name in listOf(
                "glass",
                "tinted_glass",
                "ice",
                "barrier",
                "light",
                "structure_void",
                "cave_air",
                "void_air",
                "nether_portal",
                "end_portal",
                "end_gateway",
            )) {
                Blocks.REGISTRY.idOf(Identifier.vanilla(name))?.let(::add)
            }
            for (colour in DYE_COLOURS) {
                Blocks.REGISTRY.idOf(Identifier.vanilla("${colour}_stained_glass"))?.let(::add)
            }
        }
}
