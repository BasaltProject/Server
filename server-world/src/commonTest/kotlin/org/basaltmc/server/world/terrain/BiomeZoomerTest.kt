package org.basaltmc.server.world.terrain

import kotlin.test.Test
import kotlin.test.assertEquals

class BiomeZoomerTest {
    private val zoomer = BiomeZoomer(7890956078138091352L)

    @Test
    fun selectsOfficialQuartCornersAcrossPositiveAndNegativeCoordinates() {
        assertQuart(0, 64, 0, -1, 16, 0)
        assertQuart(15, 70, 15, 4, 17, 3)
        assertQuart(-1, -64, -1, 0, -16, -1)
        assertQuart(12345, 87, -5432, 3086, 21, -1359)
    }

    private fun assertQuart(
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        expectedQuartX: Int,
        expectedQuartY: Int,
        expectedQuartZ: Int,
    ) {
        val selected = zoomer.biomeAt(blockX, blockY, blockZ, ::Triple)
        assertEquals(Triple(expectedQuartX, expectedQuartY, expectedQuartZ), selected)
    }
}
