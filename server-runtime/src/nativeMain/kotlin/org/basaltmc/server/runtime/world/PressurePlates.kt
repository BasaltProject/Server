package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import kotlin.math.ceil
import kotlin.math.floor

/**
 * The plates that answer to whatever is standing on them.
 *
 * parity: `BasePressurePlateBlock.checkPressed`: standing on one presses it and books a turn, and
 * that turn is what lets it up again once nothing is there. A wooden or stone plate is fifteen or
 * nothing; a weighted one counts what is on it against its own limit.
 *
 * What counts as standing on it is the box vanilla uses, which is narrower than the block: fourteen
 * sixteenths across and four tall, so brushing past the edge does not press it.
 */
public object PressurePlates {
    /** parity: `getPressedTime`, twenty ticks for a plain plate and ten for a weighted one. */
    private const val PRESSED_TICKS = 20
    private const val WEIGHTED_PRESSED_TICKS = 10

    /** parity: `TOUCH_AABB`, `Block.column(14, 0, 4)`: one sixteenth in on each side, a quarter tall. */
    private const val EDGE = 1.0 / 16.0
    private const val HEIGHT = 4.0 / 16.0

    /** parity: the weight each weighted plate maxes out at. */
    private const val LIGHT_MAX_WEIGHT = 15
    private const val HEAVY_MAX_WEIGHT = 150

    public fun isPlate(stateId: Int): Boolean = pathOf(stateId) != null

    /**
     * Presses or releases the plate at these coordinates, given how many things are standing on it.
     *
     * Answers the change, or null when it was already showing that. A pressed plate books its own
     * turn, which is what checks whether whatever pressed it has gone.
     */
    public fun update(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        standing: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val path = pathOf(stateId) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val wanted = signalFor(path, standing)
        val showing = properties["power"]?.toIntOrNull() ?: if (properties["powered"] == "true") MAX else 0
        if (wanted > 0) {
            world.state.scheduleBlockTick(x, y, z, block, pressedTicks(path))
        }
        if (wanted == showing) return null
        val turned =
            BlockStateDefinitions.stateIdOf(block, stateProperties(path, wanted), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /** Whether [ex]/[ey]/[ez] is inside the box that presses the plate at these coordinates. */
    public fun isStandingOn(
        x: Int,
        y: Int,
        z: Int,
        ex: Double,
        ey: Double,
        ez: Double,
    ): Boolean =
        ex >= x + EDGE &&
            ex <= x + 1 - EDGE &&
            ez >= z + EDGE &&
            ez <= z + 1 - EDGE &&
            ey >= y &&
            ey <= y + HEIGHT

    /** Which block a thing at this height is standing on: the one under its feet. */
    public fun blockUnder(ey: Double): Int = floor(ey).toInt()

    /**
     * parity: `PressurePlateBlock.getSignalStrength` and `WeightedPressurePlateBlock`'s own, which
     * scales what is on it against its limit and rounds up, so a single item already reads one.
     */
    private fun signalFor(
        path: String,
        standing: Int,
    ): Int {
        if (standing <= 0) return 0
        val maxWeight =
            when (path) {
                "light_weighted_pressure_plate" -> LIGHT_MAX_WEIGHT
                "heavy_weighted_pressure_plate" -> HEAVY_MAX_WEIGHT
                else -> return MAX
            }
        val counted = minOf(standing, maxWeight)
        return ceil(counted.toFloat() / maxWeight * MAX).toInt()
    }

    private fun stateProperties(
        path: String,
        signal: Int,
    ): Map<String, String> =
        if (path.startsWith("light_") || path.startsWith("heavy_")) {
            mapOf("power" to signal.toString())
        } else {
            mapOf("powered" to (signal > 0).toString())
        }

    private fun pressedTicks(path: String): Int =
        if (path.startsWith("light_") || path.startsWith("heavy_")) WEIGHTED_PRESSED_TICKS else PRESSED_TICKS

    private fun pathOf(stateId: Int): String? {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        return path.takeIf { it.endsWith("_pressure_plate") }
    }

    private const val MAX = 15
}
