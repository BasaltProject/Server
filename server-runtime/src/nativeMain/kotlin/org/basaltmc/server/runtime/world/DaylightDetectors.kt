package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.roundToInt

/**
 * The block that reads the sky.
 *
 * parity: `DaylightDetectorBlock.updateSignalStrength`: the sky light where it stands, bent by the
 * angle of the sun so it peaks at noon rather than following the raw light. Inverted, it answers
 * fifteen minus that and skips the curve, which is why an inverted one is a clean night switch.
 */
public object DaylightDetectors {
    /** How often vanilla lets one look, which is once every twenty ticks in its block-entity ticker. */
    public const val INTERVAL_TICKS: Int = 20

    /**
     * The power the detector at these coordinates should be showing.
     *
     * parity: `Level.getSunAngle`, which is what bends the reading: the light is at fifteen for
     * hours around noon, and the detector is not.
     */
    public fun signalFor(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        inverted: Boolean,
    ): Int {
        var target = skyBrightness(world, x, y, z)
        if (inverted) {
            target = MAX - target
        } else if (target > 0) {
            var angle = sunAngle(world) * (PI / 180.0).toFloat()
            val offset = if (angle < PI.toFloat()) 0f else (PI * 2).toFloat()
            angle += (offset - angle) * 0.2f
            target = (target * cos(angle)).roundToInt()
        }
        return target.coerceIn(0, MAX)
    }

    /** Updates the block if its reading moved, answering the change. */
    public fun update(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block.path != "daylight_detector") return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val wanted = signalFor(world, x, y, z, properties["inverted"] == "true")
        if ((properties["power"]?.toIntOrNull() ?: 0) == wanted) return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("power" to wanted.toString()), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /**
     * parity: `Level.getEffectiveSkyBrightness`, the sky light above the block minus what the
     * weather takes off it.
     */
    private fun skyBrightness(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = (WorldLight.skyLight(world, x, y + 1, z) - skyDarken(world)).coerceIn(0, MAX)

    /**
     * How much of the sky light the time of day and the weather take away.
     *
     * parity: `Level.updateSkyBrightness`, which is what makes a detector read zero at night rather
     * than the fifteen the sky light itself still says.
     */
    private fun skyDarken(world: LoadedWorld): Int {
        val rain = if (world.weather.raining) RAIN_SHARE else 1.0
        val thunder = if (world.weather.thundering) RAIN_SHARE else 1.0
        val curve = 0.5 + 2.0 * cos(timeOfDay(world) * 2.0 * PI).coerceIn(-0.25, 0.25)
        return ((1.0 - curve * rain * thunder) * DARKEN_RANGE).toInt()
    }

    /** parity: `Level.getSunAngle` in degrees, over the day's twenty-four thousand ticks. */
    private fun sunAngle(world: LoadedWorld): Float = timeOfDay(world) * DEGREES

    /** parity: `DimensionType.timeOfDay`, the curved fraction of the day everything else reads. */
    private fun timeOfDay(world: LoadedWorld): Float {
        val dayTime = (world.clock.totalTicks % DAY_TICKS).toFloat() / DAY_TICKS
        var fraction = dayTime - 0.25f
        if (fraction < 0f) fraction += 1f
        return fraction + (1f - (cos(fraction * PI.toFloat()) + 1f) / 2f - fraction) / 3f
    }

    private const val MAX = 15
    private const val DAY_TICKS = 24000L

    private const val DEGREES = 360f

    /** parity: `updateSkyBrightness`: rain and thunder each take five sixteenths off the daylight. */
    private const val RAIN_SHARE = 1.0 - 5.0 / 16.0
    private const val DARKEN_RANGE = 11.0
}
