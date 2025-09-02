package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.wire.common.Mth
import kotlin.math.E
import kotlin.math.pow

/**
 * // parity: TerrainAdjustment, narrowed to the kinds a structure this server generates asks for.
 * `bury` (stronghold, trail ruins) and `encapsulate` (trial chambers) go in with their structures.
 */
internal enum class TerrainAdjustment {
    NONE,
    BEARD_THIN,
    BEARD_BOX,
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
                    TerrainAdjustment.BEARD_THIN -> dyToGround
                    // A box clears the piece's whole height instead of bearding up from its floor,
                    // which is what lets the ancient city sit in a hollow rather than in solid rock.
                    TerrainAdjustment.BEARD_BOX -> maxOf(0, maxOf(groundY - context.blockY, context.blockY - box.maxY))
                }
            density += beardContribution(dx, dy, dz, dyToGround) * 0.8
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
