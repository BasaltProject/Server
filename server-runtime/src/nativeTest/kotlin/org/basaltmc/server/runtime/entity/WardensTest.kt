package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `AngerManagement`: a warden only comes after what it has heard enough of. */
@OptIn(ExperimentalUuidApi::class)
class WardensTest {
    private fun warden(): ServerEntity = ServerEntity(1, Uuid.random(), Wardens.TYPE, 130, 0.0, 64.0, 0.0)

    @Test
    fun aWardenThatHasHeardNothingHasNobodyToGoAfter() {
        assertNull(Wardens.target(warden()))
    }

    @Test
    fun oneNoiseIsNotEnough() {
        val warden = warden()
        Wardens.anger(warden, suspectId = 7, amount = Wardens.HEARD)
        assertEquals(35, Wardens.angerAt(warden, 7))
        assertNull(Wardens.target(warden), "thirty-five is short of the eighty it takes")
    }

    @Test
    fun enoughNoiseBringsItAfterYou() {
        val warden = warden()
        repeat(3) { Wardens.anger(warden, suspectId = 7, amount = Wardens.HEARD) }
        assertEquals(7, Wardens.target(warden))
    }

    @Test
    fun aHitIsLoudEnoughOnItsOwn() {
        val warden = warden()
        Wardens.anger(warden, suspectId = 9, amount = Wardens.HURT)
        assertEquals(9, Wardens.target(warden))
    }

    @Test
    fun angerFallsAwayAndIsForgotten() {
        val warden = warden()
        Wardens.anger(warden, suspectId = 7, amount = 3)
        // parity: one off a tick, and dropped the tick it would reach one rather than counting to zero.
        repeat(3) { Wardens.tick(warden) }
        assertEquals(0, Wardens.angerAt(warden, 7), "it is dropped rather than left at one")
        assertNull(Wardens.target(warden))
    }

    @Test
    fun angerStopsAtItsCeiling() {
        val warden = warden()
        repeat(10) { Wardens.anger(warden, suspectId = 7, amount = Wardens.HURT) }
        assertEquals(Wardens.MAX_ANGER, Wardens.angerAt(warden, 7))
    }

    @Test
    fun itGoesAfterWhoeverItIsAngriestAt() {
        val warden = warden()
        Wardens.anger(warden, suspectId = 7, amount = Wardens.ANGRY)
        Wardens.anger(warden, suspectId = 8, amount = Wardens.MAX_ANGER)
        assertEquals(8, Wardens.target(warden))
    }

    @Test
    fun onlyAWardenIsAWarden() {
        assertEquals(true, Wardens.isWarden(Wardens.TYPE))
        assertEquals(false, Wardens.isWarden(Identifier.vanilla("zombie")))
    }
}
