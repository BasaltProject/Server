package org.basaltmc.server.world.portal

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** Reads block states anywhere in a world, which is what a portal frame search needs. */
public fun interface BlockStateView {
    public fun stateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int
}

/** The two axes a Nether portal can stand on. parity: `Direction.Axis`, horizontal only. */
public enum class PortalAxis(
    public val serializedName: String,
) {
    X("x"),
    Z("z"),
    ;

    /** The direction the search calls "right": west for an X portal, south for a Z one. */
    public val rightX: Int get() = if (this == X) -1 else 0
    public val rightZ: Int get() = if (this == X) 0 else 1

    public val other: PortalAxis get() = if (this == X) Z else X
}

/**
 * A Nether portal frame, found by walking out from one block inside it.
 *
 * parity: `PortalShape`, including its two odd corners. The search starts by falling to the bottom
 * of the empty column, so lighting the top of a frame finds the same portal as lighting the bottom;
 * and "empty" means air, fire or an existing portal block, which is what lets a portal be relit
 * after part of it was broken.
 */
public class PortalShape private constructor(
    public val axis: PortalAxis,
    public val bottomLeftX: Int,
    public val bottomLeftY: Int,
    public val bottomLeftZ: Int,
    public val width: Int,
    public val height: Int,
    private val portalBlocks: Int,
) {
    public val isValid: Boolean
        get() = width in MIN_WIDTH..MAX_WIDTH && height in MIN_HEIGHT..MAX_HEIGHT

    /** Whether the frame is already full of portal blocks, which is what "this portal is lit" means. */
    public val isComplete: Boolean get() = isValid && portalBlocks == width * height

    /** Every block inside the frame, bottom row first. */
    public fun interior(): List<IntArray> =
        buildList {
            for (row in 0 until height) {
                for (column in 0 until width) {
                    add(
                        intArrayOf(
                            bottomLeftX + axis.rightX * column,
                            bottomLeftY + row,
                            bottomLeftZ + axis.rightZ * column,
                        ),
                    )
                }
            }
        }

    public companion object {
        public const val MIN_WIDTH: Int = 2
        public const val MAX_WIDTH: Int = 21
        public const val MIN_HEIGHT: Int = 3
        public const val MAX_HEIGHT: Int = 21

        private val OBSIDIAN: Int = Blocks.REGISTRY.idOf(Identifier.vanilla("obsidian")) ?: -1
        private val FIRE: Int = Blocks.REGISTRY.idOf(Identifier.vanilla("fire")) ?: -1
        private val SOUL_FIRE: Int = Blocks.REGISTRY.idOf(Identifier.vanilla("soul_fire")) ?: -1

        /** The portal block for [axis], which is what fills a frame. */
        public fun portalState(axis: PortalAxis): Int =
            Blocks.stateIdOf(Identifier.vanilla("nether_portal"), mapOf("axis" to axis.serializedName))
                ?: error("nether_portal has no ${axis.serializedName} state")

        public fun isPortal(state: Int): Boolean = Blocks.byStateId(state)?.block == Identifier.vanilla("nether_portal")

        /** The axis of a portal block, or null for anything else. */
        public fun axisOf(state: Int): PortalAxis? {
            val block = Blocks.byStateId(state) ?: return null
            if (block.block != Identifier.vanilla("nether_portal")) return null
            return if (block.properties["axis"] == "z") PortalAxis.Z else PortalAxis.X
        }

        /** parity: `PortalShape.findEmptyPortalShape`: a valid frame with nothing in it yet. */
        public fun findEmpty(
            view: BlockStateView,
            x: Int,
            y: Int,
            z: Int,
            minY: Int,
            preferred: PortalAxis = PortalAxis.X,
        ): PortalShape? = find(view, x, y, z, minY, preferred) { it.isValid && it.portalBlocks == 0 }

        /** parity: `PortalShape.findPortalShape`: the preferred axis first, then the other one. */
        public fun find(
            view: BlockStateView,
            x: Int,
            y: Int,
            z: Int,
            minY: Int,
            preferred: PortalAxis = PortalAxis.X,
            accept: (PortalShape) -> Boolean,
        ): PortalShape? {
            val first = findAnyShape(view, x, y, z, minY, preferred)
            if (accept(first)) return first
            val second = findAnyShape(view, x, y, z, minY, preferred.other)
            return if (accept(second)) second else null
        }

        /** parity: `PortalShape.findAnyShape`, which answers an invalid shape rather than nothing. */
        public fun findAnyShape(
            view: BlockStateView,
            x: Int,
            y: Int,
            z: Int,
            minY: Int,
            axis: PortalAxis,
        ): PortalShape {
            val bottom = calculateBottomLeft(view, axis, x, y, z, minY) ?: return empty(axis, x, y, z)
            val width = calculateWidth(view, axis, bottom)
            if (width == 0) return empty(axis, bottom[0], bottom[1], bottom[2])
            var portalBlocks = 0
            val height =
                calculateHeight(view, axis, bottom, width) { counted -> portalBlocks = counted }
            return PortalShape(axis, bottom[0], bottom[1], bottom[2], width, height, portalBlocks)
        }

        private fun empty(
            axis: PortalAxis,
            x: Int,
            y: Int,
            z: Int,
        ): PortalShape = PortalShape(axis, x, y, z, 0, 0, 0)

        /** Falls to the floor of the empty column, then walks left until the frame stops holding it. */
        private fun calculateBottomLeft(
            view: BlockStateView,
            axis: PortalAxis,
            x: Int,
            y: Int,
            z: Int,
            worldMinY: Int,
        ): IntArray? {
            val minY = maxOf(worldMinY, y - MAX_HEIGHT)
            var bottomY = y
            while (bottomY > minY && isEmpty(view.stateAt(x, bottomY - 1, z))) bottomY--
            val edge = distanceUntilEdgeAboveFrame(view, x, bottomY, z, -axis.rightX, -axis.rightZ) - 1
            if (edge < 0) return null
            return intArrayOf(x + -axis.rightX * edge, bottomY, z + -axis.rightZ * edge)
        }

        private fun calculateWidth(
            view: BlockStateView,
            axis: PortalAxis,
            bottomLeft: IntArray,
        ): Int {
            val width =
                distanceUntilEdgeAboveFrame(
                    view,
                    bottomLeft[0],
                    bottomLeft[1],
                    bottomLeft[2],
                    axis.rightX,
                    axis.rightZ,
                )
            return if (width in MIN_WIDTH..MAX_WIDTH) width else 0
        }

        /** How far the empty floor runs before the frame turns a corner. parity: same name in vanilla. */
        private fun distanceUntilEdgeAboveFrame(
            view: BlockStateView,
            x: Int,
            y: Int,
            z: Int,
            stepX: Int,
            stepZ: Int,
        ): Int {
            for (distance in 0..MAX_WIDTH) {
                val atX = x + stepX * distance
                val atZ = z + stepZ * distance
                val state = view.stateAt(atX, y, atZ)
                if (!isEmpty(state)) {
                    return if (state == OBSIDIAN) distance else 0
                }
                if (view.stateAt(atX, y - 1, atZ) != OBSIDIAN) return 0
            }
            return 0
        }

        private fun calculateHeight(
            view: BlockStateView,
            axis: PortalAxis,
            bottomLeft: IntArray,
            width: Int,
            portalBlocks: (Int) -> Unit,
        ): Int {
            var counted = 0
            var height = MAX_HEIGHT
            outer@ for (row in 0 until MAX_HEIGHT) {
                val y = bottomLeft[1] + row
                if (view.stateAt(bottomLeft[0] - axis.rightX, y, bottomLeft[2] - axis.rightZ) != OBSIDIAN) {
                    height = row
                    break
                }
                if (view.stateAt(
                        bottomLeft[0] + axis.rightX * width,
                        y,
                        bottomLeft[2] + axis.rightZ * width,
                    ) != OBSIDIAN
                ) {
                    height = row
                    break
                }
                for (column in 0 until width) {
                    val state =
                        view.stateAt(
                            bottomLeft[0] + axis.rightX * column,
                            y,
                            bottomLeft[2] + axis.rightZ * column,
                        )
                    if (!isEmpty(state)) {
                        height = row
                        break@outer
                    }
                    if (isPortal(state)) counted++
                }
            }
            portalBlocks(counted)
            if (height !in MIN_HEIGHT..MAX_HEIGHT) return height
            for (column in 0 until width) {
                val topX = bottomLeft[0] + axis.rightX * column
                val topZ = bottomLeft[2] + axis.rightZ * column
                if (view.stateAt(topX, bottomLeft[1] + height, topZ) != OBSIDIAN) return 0
            }
            return height
        }

        /** parity: `PortalShape.isEmpty`: air, any fire, or a portal block that is already there. */
        private fun isEmpty(state: Int): Boolean =
            Blocks.isAir(state) || state == FIRE || state == SOUL_FIRE || isPortal(state)
    }
}
