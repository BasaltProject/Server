package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.wire.common.Mth
import kotlin.math.E
import kotlin.math.pow
import kotlin.math.sqrt

/**
 * // parity: TerrainAdjustment, all four of the jar's kinds.
 */
internal enum class TerrainAdjustment {
    NONE,
    BURY,
    BEARD_THIN,
    BEARD_BOX,
    ENCAPSULATE,
}

/** // parity: Beardifier's rigid-piece and junction contributions. */

internal class StructureBeardifier(
    private val pieces: List<Rigid>,
    private val junctions: List<Junction> = emptyList(),
) : DensityFunction {
    override fun compute(context: FunctionContext): Double {
        var density = 0.0
        for (rigid in pieces) {
            val box = rigid.box
            val dx = maxOf(0, maxOf(box.minX - context.blockX, context.blockX - box.maxX))
            val dz = maxOf(0, maxOf(box.minZ - context.blockZ, context.blockZ - box.maxZ))
            val groundY = box.minY + rigid.groundLevelDelta
            val dyToGround = context.blockY - groundY
            val dy =
                when (rigid.adjustment) {
                    TerrainAdjustment.NONE -> continue
                    TerrainAdjustment.BURY, TerrainAdjustment.BEARD_THIN -> dyToGround
                    // A box clears the piece's whole height instead of bearding up from its floor,
                    // which is what lets the ancient city sit in a hollow rather than in solid rock.
                    TerrainAdjustment.BEARD_BOX -> maxOf(0, maxOf(groundY - context.blockY, context.blockY - box.maxY))
                    TerrainAdjustment.ENCAPSULATE ->
                        maxOf(0, maxOf(box.minY - context.blockY, context.blockY - box.maxY))
                }
            density +=
                when (rigid.adjustment) {
                    // parity: encapsulate ADDS density instead of carving, so the structure ends up
                    // cased in rock rather than sitting in a hollow. Halving each axis first is
                    // what stretches that casing out to roughly twelve blocks.
                    TerrainAdjustment.ENCAPSULATE -> buryContribution(dx / 2.0, dy / 2.0, dz / 2.0) * 0.8
                    // parity: bury halves only the vertical axis and takes no 0.8, so it fills the
                    // ground back over the piece rather than clearing a hollow under it.
                    TerrainAdjustment.BURY -> buryContribution(dx.toDouble(), dy / 2.0, dz.toDouble())
                    else -> beardContribution(dx, dy, dz, dyToGround) * 0.8
                }
        }
        for (junction in junctions) {
            val dx = context.blockX - junction.x
            val dy = context.blockY - junction.groundY
            val dz = context.blockZ - junction.z
            density += beardContribution(dx, dy, dz, dy) * 0.4
        }
        return density
    }

    override fun minValue(): Double = Double.NEGATIVE_INFINITY

    override fun maxValue(): Double = Double.POSITIVE_INFINITY

    internal data class Rigid(
        val box: StructureBoundingBox,
        val groundLevelDelta: Int = 0,
        val adjustment: TerrainAdjustment = TerrainAdjustment.BEARD_THIN,
    )

    internal data class Junction(
        val x: Int,
        val groundY: Int,
        val z: Int,
    )

    private companion object {
        const val KERNEL_RADIUS = 12
        const val KERNEL_SIZE = 24
        val KERNEL =
            FloatArray(KERNEL_SIZE * KERNEL_SIZE * KERNEL_SIZE) { index ->
                val z = index / (KERNEL_SIZE * KERNEL_SIZE) - KERNEL_RADIUS
                val x = index / KERNEL_SIZE % KERNEL_SIZE - KERNEL_RADIUS
                val y = index % KERNEL_SIZE - KERNEL_RADIUS
                E.pow(-(x * x + (y + 0.5) * (y + 0.5) + z * z) / 16.0).toFloat()
            }

        /** // parity: Beardifier.getBuryContribution, a straight falloff from 1 to 0 over six blocks. */
        fun buryContribution(
            dx: Double,
            dy: Double,
            dz: Double,
        ): Double = Mth.clampedMap(sqrt(dx * dx + dy * dy + dz * dz), 0.0, 6.0, 1.0, 0.0)

        fun beardContribution(
            dx: Int,
            dy: Int,
            dz: Int,
            yToGround: Int,
        ): Double {
            val xi = dx + KERNEL_RADIUS
            val yi = dy + KERNEL_RADIUS
            val zi = dz + KERNEL_RADIUS
            if (xi !in 0 until KERNEL_SIZE || yi !in 0 until KERNEL_SIZE || zi !in 0 until KERNEL_SIZE) {
                return 0.0
            }
            val offsetY = yToGround + 0.5
            val distanceSquared = dx * dx + offsetY * offsetY + dz * dz
            val value = -offsetY * Mth.fastInvSqrt(distanceSquared / 2.0) / 2.0
            return value * KERNEL[zi * KERNEL_SIZE * KERNEL_SIZE + xi * KERNEL_SIZE + yi]
        }
    }
}
