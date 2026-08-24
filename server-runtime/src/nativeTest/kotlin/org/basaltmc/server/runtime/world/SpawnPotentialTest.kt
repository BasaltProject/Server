package org.basaltmc.server.runtime.world

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `PotentialCalculator`, used by the Nether biomes that declare spawn costs. */
class SpawnPotentialTest {
    @Test
    fun chargesFallOffByEuclideanDistanceAndMultiplyTheCandidateCharge() {
        val potential = SpawnPotential()
        potential.add(0, 0, 0, charge = 1.0)

        assertEquals(0.4, potential.energyChange(3, 4, 0, charge = 2.0))
        assertTrue(potential.allows(3, 4, 0, MobSpawnCost(energyBudget = 0.4, charge = 2.0)))
        assertFalse(potential.allows(3, 4, 0, MobSpawnCost(energyBudget = 0.399, charge = 2.0)))
    }

    @Test
    fun aChargeAtTheSameBlockHasInfinitePotentialAndZeroChargesDoNothing() {
        val potential = SpawnPotential()
        potential.add(7, 20, -3, charge = 0.0)
        assertTrue(potential.allows(7, 20, -3, MobSpawnCost(energyBudget = 0.0, charge = 1.0)))

        potential.add(7, 20, -3, charge = 0.7)
        assertEquals(Double.POSITIVE_INFINITY, potential.energyChange(7, 20, -3, charge = 0.7))
        assertFalse(potential.allows(7, 20, -3, MobSpawnCost(energyBudget = 100.0, charge = 0.7)))
    }
}
