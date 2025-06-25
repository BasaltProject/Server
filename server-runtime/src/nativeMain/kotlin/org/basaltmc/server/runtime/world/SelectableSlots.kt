package org.basaltmc.server.runtime.world

import kotlin.math.floor

/**
 * Which slot of a block a click landed on, worked out from where on the face it hit.
 *
 * parity: `SelectableSlotContainer`. Two blocks use it, the chiseled bookshelf and the shelf, and
 * both only answer to a click on the face they look out of: a bookshelf poked from the side takes
 * nothing.
 */
public object SelectableSlots {
    /**
     * The slot at [clickX]/[clickY]/[clickZ] on the [face] of a block looking [facing], or null when
     * the click was not on that face at all.
     *
     * The coordinates are the ones the client sends, relative to the block itself, which is what
     * vanilla arrives at after subtracting the neighbouring position from the hit.
     */
    public fun hitSlot(
        facing: String,
        face: String,
        clickX: Float,
        clickY: Float,
        clickZ: Float,
        rows: Int,
        columns: Int,
    ): Int? {
        if (face != facing) return null
        val across =
            when (face) {
                "north" -> 1f - clickX
                "south" -> clickX
                "west" -> clickZ
                "east" -> 1f - clickZ
                else -> return null
            }
        val row = section(1f - clickY, rows)
        val column = section(across, columns)
        return column + row * columns
    }

    /** parity: `getSection`, which counts in sixteenths of a block however many slots there are. */
    private fun section(
        along: Float,
        sections: Int,
    ): Int {
        val pixel = along * 16f
        val size = 16f / sections
        return floor(pixel / size).toInt().coerceIn(0, sections - 1)
    }
}
