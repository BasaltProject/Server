package org.basaltmc.world.terrain

import org.basaltmc.common.Mth
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.structure.StructureBoundingBox
import kotlin.math.E
import kotlin.math.pow

/** // parity: Beardifier's BEARD_THIN rigid-piece contribution, used by Nether fossils. */
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
            val dyToGround = context.blockY - (box.minY + rigid.groundLevelDelta)
            density += beardContribution(dx, dyToGround, dz, dyToGround) * 0.8
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
