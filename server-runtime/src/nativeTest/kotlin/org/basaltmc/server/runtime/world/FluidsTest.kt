package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `LivingEntity.baseTick`'s air supply and `Entity.lavaHurt`, which the HUD draws. */
class FluidsTest {
    @Test
    fun waterAndLavaAreTheTwoThingsAPlayerCanBeUnder() {
        assertEquals(Submersion.WATER, Fluids.of(Blocks.WATER.stateId))
        assertEquals(Submersion.LAVA, Fluids.of(Blocks.LAVA.stateId))
        assertEquals(Submersion.NONE, Fluids.of(Blocks.AIR.stateId))
        assertEquals(
            Submersion.NONE,
            Fluids.of(checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone")))),
        )
    }

    /** Three hundred ticks of air, one lost per tick under water, four regained per tick out of it. */
    @Test
    fun airRunsDownOneTickAtATimeAndComesBackFourAtATime() {
        var air = Fluids.MAX_AIR
        repeat(10) { air = Fluids.airAfterTick(air, eyesInWater = true) }
        assertEquals(290, air)

        repeat(2) { air = Fluids.airAfterTick(air, eyesInWater = false) }
        assertEquals(298, air)

        // It never goes past full.
        repeat(10) { air = Fluids.airAfterTick(air, eyesInWater = false) }
        assertEquals(Fluids.MAX_AIR, air)
    }

    /** parity: `shouldTakeDrowningDamage`: twenty ticks past empty before the first hit. */
    @Test
    fun drowningStartsTwentyTicksAfterTheAirRunsOut() {
        var air = 0
        var ticksToDamage = 0
        while (air > Fluids.DROWNING_AIR) {
            air = Fluids.airAfterTick(air, eyesInWater = true)
            ticksToDamage++
        }

        assertEquals(20, ticksToDamage)
        assertEquals(Fluids.DROWNING_AIR, air)
    }

    @Test
    fun sneakingLowersWhereTheEyesAre() {
        assertEquals(1.62, Fluids.eyeHeight(sneaking = false))
        assertEquals(1.27, Fluids.eyeHeight(sneaking = true))
    }
}
