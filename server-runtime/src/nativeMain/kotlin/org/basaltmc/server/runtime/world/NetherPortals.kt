package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.portal.BlockStateView
import org.basaltmc.server.world.portal.PortalAxis
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/** Where a portal leaves someone: the block they arrive on, and which world it is in. */
public data class PortalExit(
    public val world: LoadedWorld,
    public val x: Double,
    public val y: Double,
    public val z: Double,
)

/**
 * Lighting a Nether portal, and finding or building the one on the other side.
 *
 * parity: `PortalShape` for the frame, `PortalForcer` for the exit. The one deliberate difference
 * is where the search for an existing portal looks: vanilla asks a point-of-interest index written
 * as chunks generate, and this build has none, so it reads the chunks themselves and only sees the
 * ones already loaded. What it costs is a duplicate portal when the old one is out of memory; what
 * it saves is generating a few hundred chunks to answer one question.
 */
public object NetherPortals {
    /** parity: `Entity.getDimensionChangingDelay`, the 300 ticks that stop a portal loop. */
    public const val TRAVEL_COOLDOWN_TICKS: Int = 300

    /** parity: `PortalForcer`'s two radii: the Nether side is searched much closer in. */
    private const val NETHER_SEARCH_RADIUS = 16
    private const val OVERWORLD_SEARCH_RADIUS = 128
    private const val CREATE_SEARCH_RADIUS = 16

    private val OBSIDIAN = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("obsidian")))
    private val FIRE = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("fire")))

    /** Reads a world's blocks, generating nothing: what the frame search around a player needs. */
    public fun view(world: LoadedWorld): BlockStateView =
        BlockStateView { x, y, z -> world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId }

    /**
     * Lights the frame [x, y, z] sits in, filling it with portal blocks.
     *
     * parity: `BaseFireBlock.onPlace`, which is why this happens where the fire would go rather
     * than where the flint and steel was aimed.
     */
    public fun light(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): PortalShape? {
        val shape = PortalShape.findEmpty(view(world), x, y, z, world.minY) ?: return null
        val state = PortalShape.portalState(shape.axis)
        for (block in shape.interior()) world.state.setBlockState(block[0], block[1], block[2], state)
        return shape
    }

    /** The fire a flint and steel leaves when there is no frame to light. */
    public fun placeFire(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = world.state.setBlockState(x, y, z, FIRE)

    /**
     * Puts out any portal next to [x, y, z] whose frame no longer holds it, and answers the blocks
     * that went out.
     *
     * parity: `NetherPortalBlock.updateShape`, which turns a portal block into air the moment its
     * own shape stops being complete. Vanilla lets that cascade one neighbour at a time through
     * block updates; this walks the connected blocks itself, which ends in the same place.
     */
    public fun extinguishAround(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val cleared = mutableListOf<IntArray>()
        for (side in NEIGHBOURS) {
            val blockX = x + side[0]
            val blockY = y + side[1]
            val blockZ = z + side[2]
            val state = world.state.getBlockState(blockX, blockY, blockZ) ?: continue
            val axis = PortalShape.axisOf(state) ?: continue
            val shape = PortalShape.findAnyShape(view(world), blockX, blockY, blockZ, world.minY, axis)
            if (shape.isComplete) continue
            cleared += clearConnected(world, blockX, blockY, blockZ)
        }
        return cleared
    }

    /** Every portal block reachable from this one, turned back into air. */
    private fun clearConnected(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val cleared = mutableListOf<IntArray>()
        val pending = ArrayDeque<IntArray>()
        pending.addLast(intArrayOf(x, y, z))
        while (pending.isNotEmpty()) {
            val block = pending.removeFirst()
            val state = world.state.getBlockState(block[0], block[1], block[2]) ?: continue
            if (!PortalShape.isPortal(state)) continue
            world.state.setBlockState(block[0], block[1], block[2], Blocks.AIR.stateId)
            cleared += block
            for (side in NEIGHBOURS) {
                pending.addLast(intArrayOf(block[0] + side[0], block[1] + side[1], block[2] + side[2]))
            }
        }
        return cleared
    }

    /**
     * Where someone standing in a portal at [x, y, z] of [from] comes out in [to].
     *
     * parity: `NetherPortalBlock.getPortalDestination`. The horizontal position scales by the ratio
     * of the two dimensions' coordinate scales (the 1:8), the vertical does not, and an existing
     * portal near the scaled position wins over building a new one.
     */
    public fun exitFor(
        from: LoadedWorld,
        to: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
        axis: PortalAxis,
    ): PortalExit? {
        val scale = from.declaration.dimension.coordinateScale / to.declaration.dimension.coordinateScale
        val targetX = floor(x * scale).toInt()
        val targetZ = floor(z * scale).toInt()
        val targetY = y.toInt().coerceIn(to.minY + 1, to.maxY - 3)
        val toNether = to.declaration.dimension == org.basaltmc.server.runtime.config.WorldDimension.NETHER
        val radius = if (toNether) NETHER_SEARCH_RADIUS else OVERWORLD_SEARCH_RADIUS

        val existing = findClosestPortal(to, targetX, targetY, targetZ, radius)
        val landing = existing ?: createPortal(to, targetX, targetY, targetZ, axis) ?: return null
        return PortalExit(to, landing[0] + 0.5, landing[1].toDouble(), landing[2] + 0.5)
    }

    /**
     * The nearest portal block to [x, y, z], in the chunks this world already has in memory.
     *
     * The palette check is what makes it affordable: a section that holds no portal block at all is
     * skipped without reading one, the same trick `/locate poi` uses.
     */
    public fun findClosestPortal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        radius: Int,
    ): IntArray? {
        var best: IntArray? = null
        var bestDistance = Long.MAX_VALUE
        for (chunkX in ((x - radius) shr 4)..((x + radius) shr 4)) {
            for (chunkZ in ((z - radius) shr 4)..((z + radius) shr 4)) {
                val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: continue
                for (index in 0 until chunk.sectionCount) {
                    val section = chunk.section(index)
                    if (!section.mayContain { PortalShape.isPortal(it) }) continue
                    val sectionMinY = chunk.verticalBounds.minY + index * SECTION_HEIGHT
                    for (localY in 0 until SECTION_HEIGHT) {
                        for (localZ in 0 until 16) {
                            for (localX in 0 until 16) {
                                if (!PortalShape.isPortal(section.getBlockStateId(localX, localY, localZ))) continue
                                val blockX = chunkX * 16 + localX
                                val blockY = sectionMinY + localY
                                val blockZ = chunkZ * 16 + localZ
                                if (blockX !in x - radius..x + radius) continue
                                if (blockZ !in z - radius..z + radius) continue
                                val dx = (blockX - x).toLong()
                                val dy = (blockY - y).toLong()
                                val dz = (blockZ - z).toLong()
                                val distance = dx * dx + dy * dy + dz * dz
                                if (distance >= bestDistance) continue
                                bestDistance = distance
                                best = intArrayOf(blockX, blockY, blockZ)
                            }
                        }
                    }
                }
            }
        }
        return best
    }

    /**
     * Builds a portal near [x, y, z] and answers the block to arrive on.
     *
     * parity: `PortalForcer.createPortal`, in its shape rather than its search: a 4 by 5 obsidian
     * frame around a 2 by 3 opening, on the first column with room for it, and if there is none, at
     * a clamped height on a platform of its own, which is how vanilla always manages to make one.
     */
    public fun createPortal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        axis: PortalAxis,
    ): IntArray? {
        val maxY = min(world.maxY, world.minY + world.declaration.dimension.chunkHeight.height - 1)
        val spot = findFrameSpot(world, x, y, z, axis, maxY) ?: forceFrameSpot(world, x, y, z, axis, maxY)
        buildFrame(world, spot, axis)
        return intArrayOf(spot[0], spot[1], spot[2])
    }

    /** The right-hand step of the frame, which is the axis it stands on. */
    private fun step(axis: PortalAxis): IntArray = intArrayOf(axis.rightX, axis.rightZ)

    /** Walks out from the origin looking for a column with a floor and four blocks of room. */
    private fun findFrameSpot(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        axis: PortalAxis,
        maxY: Int,
    ): IntArray? {
        var best: IntArray? = null
        var bestDistance = Long.MAX_VALUE
        for (offsetX in -CREATE_SEARCH_RADIUS..CREATE_SEARCH_RADIUS) {
            for (offsetZ in -CREATE_SEARCH_RADIUS..CREATE_SEARCH_RADIUS) {
                val columnX = x + offsetX
                val columnZ = z + offsetZ
                val chunk = world.state.getOrGenerateChunk(columnX shr 4, columnZ shr 4)
                val surface = min(maxY - 5, chunk.heightAt(columnX and 15, columnZ and 15))
                if (surface <= world.minY) continue
                val candidate = intArrayOf(columnX, surface + 1, columnZ)
                if (!canHostFrame(world, candidate, axis)) continue
                val dx = offsetX.toLong()
                val dz = offsetZ.toLong()
                val dy = (candidate[1] - y).toLong()
                val distance = dx * dx + dz * dz + dy * dy
                if (distance >= bestDistance) continue
                bestDistance = distance
                best = candidate
            }
        }
        return best
    }

    /** parity: `PortalForcer`'s last resort, which clamps the height and lays its own floor. */
    private fun forceFrameSpot(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        axis: PortalAxis,
        maxY: Int,
    ): IntArray {
        val spot = intArrayOf(x, y.coerceIn(max(world.minY + 1, FORCED_MIN_Y), maxY - 9), z)
        val (stepX, stepZ) = step(axis)
        for (side in -1..1) {
            for (column in 0 until 2) {
                for (row in -1 until 3) {
                    val state = if (row < 0) OBSIDIAN else Blocks.AIR.stateId
                    world.state.setBlockState(
                        spot[0] + stepX * column + stepZ * side,
                        spot[1] + row,
                        spot[2] + stepZ * column - stepX * side,
                        state,
                    )
                }
            }
        }
        return spot
    }

    /** parity: `PortalForcer.canHostFrame`: solid under the frame, replaceable everywhere in it. */
    private fun canHostFrame(
        world: LoadedWorld,
        spot: IntArray,
        axis: PortalAxis,
    ): Boolean {
        val (stepX, stepZ) = step(axis)
        for (column in -1 until 3) {
            for (row in -1 until 4) {
                val blockX = spot[0] + stepX * column
                val blockY = spot[1] + row
                val blockZ = spot[2] + stepZ * column
                if (blockY !in world.minY..world.maxY) return false
                val state = world.state.getBlockState(blockX, blockY, blockZ) ?: return false
                if (row < 0 && Blocks.isAir(state)) return false
                if (row >= 0 && !Blocks.isAir(state)) return false
            }
        }
        return true
    }

    /** The obsidian ring and the portal blocks inside it. */
    private fun buildFrame(
        world: LoadedWorld,
        spot: IntArray,
        axis: PortalAxis,
    ) {
        val (stepX, stepZ) = step(axis)
        for (column in -1 until 3) {
            for (row in -1 until 4) {
                if (column != -1 && column != 2 && row != -1 && row != 3) continue
                world.state.setBlockState(
                    spot[0] + stepX * column,
                    spot[1] + row,
                    spot[2] + stepZ * column,
                    OBSIDIAN,
                )
            }
        }
        val portal = PortalShape.portalState(axis)
        for (column in 0 until 2) {
            for (row in 0 until 3) {
                world.state.setBlockState(spot[0] + stepX * column, spot[1] + row, spot[2] + stepZ * column, portal)
            }
        }
    }

    private val NEIGHBOURS =
        listOf(
            intArrayOf(1, 0, 0),
            intArrayOf(-1, 0, 0),
            intArrayOf(0, 1, 0),
            intArrayOf(0, -1, 0),
            intArrayOf(0, 0, 1),
            intArrayOf(0, 0, -1),
        )

    private const val SECTION_HEIGHT = 16
    private const val FORCED_MIN_Y = 70
}

/** The world's own vertical bounds, which a portal has to stay inside. */
internal val LoadedWorld.minY: Int get() = declaration.dimension.chunkHeight.minY

internal val LoadedWorld.maxY: Int
    get() = declaration.dimension.chunkHeight.minY + declaration.dimension.chunkHeight.height - 1
