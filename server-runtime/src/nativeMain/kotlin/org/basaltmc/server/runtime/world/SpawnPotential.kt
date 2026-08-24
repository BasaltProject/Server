package org.basaltmc.server.runtime.world

import kotlin.math.sqrt

/**
 * The biome-defined repulsion field that keeps costly mobs from packing into one Nether pocket.
 *
 * parity: `PotentialCalculator`: every existing charge contributes `charge / distance`; a proposed
 * mob multiplies that field by its own charge and compares it to its biome's energy budget.
 */
public class SpawnPotential {
    private val charges = mutableListOf<PointCharge>()

    public fun add(
        x: Int,
        y: Int,
        z: Int,
        charge: Double,
    ) {
        if (charge != 0.0) charges += PointCharge(x, y, z, charge)
    }

    public fun allows(
        x: Int,
        y: Int,
        z: Int,
        cost: MobSpawnCost,
    ): Boolean = energyChange(x, y, z, cost.charge) <= cost.energyBudget

    internal fun energyChange(
        x: Int,
        y: Int,
        z: Int,
        charge: Double,
    ): Double {
        if (charge == 0.0) return 0.0
        var potential = 0.0
        for (point in charges) {
            val dx = (point.x - x).toDouble()
            val dy = (point.y - y).toDouble()
            val dz = (point.z - z).toDouble()
            val distanceSquared = dx * dx + dy * dy + dz * dz
            if (distanceSquared == 0.0) return Double.POSITIVE_INFINITY
            potential += point.charge / sqrt(distanceSquared)
        }
        return potential * charge
    }

    private class PointCharge(
        val x: Int,
        val y: Int,
        val z: Int,
        val charge: Double,
    )
}
