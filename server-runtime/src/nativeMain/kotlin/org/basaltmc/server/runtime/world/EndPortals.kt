package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * The end portal: arming a frame ring with eyes, and what happens to whoever steps into it.
 *
 * parity: `EnderEyeItem.useOn` for the eye, `EndPortalFrameBlock.getOrCreatePortalShape` for the
 * ring, `EndPortalBlock.getPortalDestination` for the travel and `EndPlatformFeature` for the
 * obsidian slab everyone lands on. The ring is checked by hand rather than through a general block
 * pattern matcher: one pattern is not a matcher.
 */
public object EndPortals {
    /** parity: `ServerLevel.END_SPAWN_POINT`, the block everyone arrives at. */
    public const val SPAWN_X: Int = 100
    public const val SPAWN_Y: Int = 50
    public const val SPAWN_Z: Int = 0

    private val FRAME = Identifier.vanilla("end_portal_frame")
    private val END_PORTAL = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("end_portal")))
    private val OBSIDIAN = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("obsidian")))

    /** Where the frames sit around a centre, and which way each of them has to look. */
    private val RING: List<IntArray> =
        buildList {
            for (offset in -1..1) {
                add(intArrayOf(offset, -2, SOUTH))
                add(intArrayOf(offset, 2, NORTH))
                add(intArrayOf(-2, offset, EAST))
                add(intArrayOf(2, offset, WEST))
            }
        }

    public fun isFrame(state: Int): Boolean = Blocks.byStateId(state)?.block == FRAME

    public fun isEndPortal(state: Int): Boolean = state == END_PORTAL

    /** Whether this frame is still waiting for its eye. */
    public fun needsEye(state: Int): Boolean {
        val block = Blocks.byStateId(state) ?: return false
        return block.block == FRAME && block.properties["eye"] != "true"
    }

    /**
     * Puts an eye in the frame at [x, y, z], and fills the ring if that was the last one.
     *
     * Returns the blocks that changed, so the caller can tell the clients about them, or null when
     * there was no frame to arm.
     */
    public fun placeEye(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray>? {
        val state = world.state.getBlockState(x, y, z) ?: return null
        if (!needsEye(state)) return null
        val facing = Blocks.byStateId(state)?.properties?.get("facing") ?: "north"
        val armed =
            Blocks.stateIdOf(FRAME, mapOf("eye" to "true", "facing" to facing)) ?: return null
        world.state.setBlockState(x, y, z, armed)
        val changed = mutableListOf(intArrayOf(x, y, z, armed))
        completeRing(world, x, y, z)?.let { changed += it }
        return changed
    }

    /**
     * Fills the ring [x, y, z] belongs to, if all twelve frames are armed and facing the middle.
     *
     * parity: the frames look inward, which is what the stronghold's own portal room builds and
     * what the pattern in `EndPortalFrameBlock` matches.
     */
    public fun completeRing(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray>? {
        for (slot in RING) {
            val centreX = x - slot[0]
            val centreZ = z - slot[1]
            if (!isRingComplete(world, centreX, y, centreZ)) continue
            val filled = mutableListOf<IntArray>()
            for (offsetX in -1..1) {
                for (offsetZ in -1..1) {
                    val blockX = centreX + offsetX
                    val blockZ = centreZ + offsetZ
                    world.state.setBlockState(blockX, y, blockZ, END_PORTAL)
                    filled += intArrayOf(blockX, y, blockZ, END_PORTAL)
                }
            }
            return filled
        }
        return null
    }

    private fun isRingComplete(
        world: LoadedWorld,
        centreX: Int,
        y: Int,
        centreZ: Int,
    ): Boolean =
        RING.all { slot ->
            val state = world.state.getBlockState(centreX + slot[0], y, centreZ + slot[1]) ?: return@all false
            val block = Blocks.byStateId(state) ?: return@all false
            block.block == FRAME &&
                block.properties["eye"] == "true" &&
                block.properties["facing"] == FACING_NAMES[slot[2]]
        }

    /** parity: `EndPlatformFeature.createEndPlatform`: five by five of obsidian, three of air over it. */
    public fun createEndPlatform(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ) {
        for (offsetX in -2..2) {
            for (offsetZ in -2..2) {
                world.state.getOrGenerateChunk((x + offsetX) shr 4, (z + offsetZ) shr 4)
                for (offsetY in -1 until 3) {
                    val state = if (offsetY == -1) OBSIDIAN else Blocks.AIR.stateId
                    world.state.setBlockState(x + offsetX, y + offsetY, z + offsetZ, state)
                }
            }
        }
    }

    /**
     * Where an end portal in [from] leaves someone.
     *
     * parity: `EndPortalBlock.getPortalDestination`. Going out builds the platform and drops the
     * player on it; coming back uses the world's own spawn, which is what vanilla's respawn data
     * amounts to while there are no beds.
     */
    public fun exitFor(
        from: LoadedWorld,
        to: LoadedWorld,
    ): PortalExit {
        val fromEnd = from.declaration.dimension == org.basaltmc.server.runtime.config.WorldDimension.END
        if (fromEnd) {
            val (spawnX, spawnY, spawnZ) = to.spawn
            return PortalExit(to, spawnX, spawnY, spawnZ)
        }
        createEndPlatform(to, SPAWN_X, SPAWN_Y, SPAWN_Z)
        return PortalExit(to, SPAWN_X + 0.5, SPAWN_Y.toDouble(), SPAWN_Z + 0.5)
    }

    private const val NORTH = 0
    private const val SOUTH = 1
    private const val WEST = 2
    private const val EAST = 3
    private val FACING_NAMES = arrayOf("north", "south", "west", "east")
}
