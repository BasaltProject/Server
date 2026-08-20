package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `Mob.burnUndead` over `isSunBurnTick`. */
class SunBurnTest {
    private fun burnsOverTwoSeconds(
        type: String,
        day: Boolean,
        light: Int,
        openSky: Boolean,
        inWater: Boolean = false,
    ): Boolean {
        val random = LegacyRandom(9L)
        repeat(40) {
            if (SunBurn.shouldBurn(Identifier.vanilla(type), day, light, openSky, inWater, random)) return true
        }
        return false
    }

    @Test
    fun aZombieInTheOpenAtNoonCatchesFire() {
        assertTrue(burnsOverTwoSeconds("zombie", day = true, light = 15, openSky = true))
    }

    @Test
    fun nightLeavesItAlone() {
        assertFalse(burnsOverTwoSeconds("zombie", day = false, light = 15, openSky = true))
    }

    @Test
    fun aRoofOverItIsEnough() {
        assertFalse(burnsOverTwoSeconds("zombie", day = true, light = 15, openSky = false))
    }

    @Test
    fun waterPutsItOut() {
        assertFalse(burnsOverTwoSeconds("zombie", day = true, light = 15, openSky = true, inWater = true))
    }

    @Test
    fun dimLightIsNotEnough() {
        assertFalse(burnsOverTwoSeconds("zombie", day = true, light = 7, openSky = true))
    }

    @Test
    fun onlyTheUndeadBurn() {
        assertFalse(burnsOverTwoSeconds("cow", day = true, light = 15, openSky = true))
        assertFalse(burnsOverTwoSeconds("creeper", day = true, light = 15, openSky = true))
        assertTrue(burnsOverTwoSeconds("skeleton", day = true, light = 15, openSky = true))
    }
}
