package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.BlockHit
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max

/**
 * What a block does when something hits it in flight.
 *
 * parity: the ten `onProjectileHit` overrides in the jar. The one that matters to redstone is the
 * target block, whose signal depends on *where* it was hit: dead centre is fifteen, the rim is one.
 */
public object ProjectileHits {
    /** parity: `TargetBlock.updateRedstoneOutput`, twenty ticks for an arrow and eight for the rest. */
    public const val ARROW_HOLD_TICKS: Int = 20
    public const val THROWN_HOLD_TICKS: Int = 8

    private val TARGET = Identifier.vanilla("target")

    /**
     * Answers what changed, if anything. [byArrow] decides how long a target holds its signal, which
     * is the only thing the kind of projectile changes here.
     */
    public fun onHit(
        world: LoadedWorld,
        hit: BlockHit,
        byArrow: Boolean,
    ): BlockChange? {
        val stateId = world.state.getBlockState(hit.blockX, hit.blockY, hit.blockZ) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        return when (block.path) {
            "target" -> hitTarget(world, hit, stateId, block, byArrow)
            // parity: `ChorusFlowerBlock` and `DecoratedPotBlock`, which simply break.
            "chorus_flower", "decorated_pot" -> shatter(world, hit)
            "big_dripleaf" -> tiltDripleaf(world, hit, stateId, block)
            else -> null
        }
    }

    /**
     * parity: `TargetBlock.getRedstoneStrength`: the distance from the middle of the face that was
     * hit, over the half block that face is wide, is what decides the signal.
     */
    public fun signalFor(hit: BlockHit): Int {
        val fracX = abs(fraction(hit.x) - HALF)
        val fracY = abs(fraction(hit.y) - HALF)
        val fracZ = abs(fraction(hit.z) - HALF)
        val distance =
            when (hit.face.axis) {
                1 -> max(fracX, fracZ)
                2 -> max(fracX, fracY)
                else -> max(fracY, fracZ)
            }
        val scaled = ((HALF - distance) / HALF).coerceIn(0.0, 1.0)
        return max(1, ceil(MAX_SIGNAL * scaled).toInt())
    }

    private fun hitTarget(
        world: LoadedWorld,
        hit: BlockHit,
        stateId: Int,
        block: Identifier,
        byArrow: Boolean,
    ): BlockChange? {
        // parity: a target already counting down keeps the signal it has; only the first hit sets it.
        if (world.state.hasScheduledBlockTick(hit.blockX, hit.blockY, hit.blockZ, TARGET)) return null
        val signal = signalFor(hit)
        val turned =
            BlockStateDefinitions.stateIdOf(block, mapOf("power" to signal.toString()), stateId) ?: return null
        if (!world.state.setBlockState(hit.blockX, hit.blockY, hit.blockZ, turned)) return null
        world.state.scheduleBlockTick(
            hit.blockX,
            hit.blockY,
            hit.blockZ,
            TARGET,
            if (byArrow) ARROW_HOLD_TICKS else THROWN_HOLD_TICKS,
        )
        return BlockChange(hit.blockX, hit.blockY, hit.blockZ, turned)
    }

    /** The other half of a target: the scheduled tick that lets its signal go. */
    public fun releaseTarget(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block.path != "target") return null
        if ((BlockStateDefinitions.propertiesOf(stateId)["power"]?.toIntOrNull() ?: 0) == 0) return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("power" to "0"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    private fun shatter(
        world: LoadedWorld,
        hit: BlockHit,
    ): BlockChange? {
        if (!world.state.setBlockState(hit.blockX, hit.blockY, hit.blockZ, Blocks.AIR.stateId)) return null
        return BlockChange(hit.blockX, hit.blockY, hit.blockZ, Blocks.AIR.stateId)
    }

    /** parity: `BigDripleafBlock.onProjectileHit`, which tips it right over. */
    private fun tiltDripleaf(
        world: LoadedWorld,
        hit: BlockHit,
        stateId: Int,
        block: Identifier,
    ): BlockChange? {
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("tilt" to "full"), stateId) ?: return null
        if (turned == stateId) return null
        if (!world.state.setBlockState(hit.blockX, hit.blockY, hit.blockZ, turned)) return null
        return BlockChange(hit.blockX, hit.blockY, hit.blockZ, turned)
    }

    /** parity: `Mth.frac`, which is the part of a coordinate inside its own block. */
    private fun fraction(value: Double): Double = value - floor(value)

    private const val HALF = 0.5
    private const val MAX_SIGNAL = 15.0
}

/** The axis a face runs along, as `TargetBlock` reads it: 0 for X, 1 for Y, 2 for Z. */
private val BlockFace.axis: Int
    get() =
        when (this) {
            BlockFace.UP, BlockFace.DOWN -> 1
            BlockFace.NORTH, BlockFace.SOUTH -> 2
            else -> 0
        }
