package org.basaltmc.server.runtime.sched

import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity of model with `RegionizedServer`: one owner per piece of state, everyone else asks. */
class OwnerTest {
    @AfterTest
    fun stopEnforcing() {
        Ownership.strict = false
    }

    @Test
    fun theThreadThatBoundItIsTheOwner() {
        val owner = Owner("world")
        assertFalse(owner.isCurrent, "nobody owns it until somebody claims it")

        owner.bindToCurrentThread()

        assertTrue(owner.isCurrent)
    }

    @Test
    fun theOwnerRunsItsOwnTasksStraightAway() {
        val owner = Owner("world")
        owner.bindToCurrentThread()
        var ran = false

        owner.submit { ran = true }

        assertTrue(ran, "the owner does not queue behind itself")
        assertEquals(0, owner.pending)
    }

    @Test
    fun anotherThreadQueuesAndTheOwnerRunsIt() {
        val owner = Owner("world")
        owner.bindToCurrentThread()
        val ran = AtomicInt(0)
        val submitted = AtomicInt(0)

        NativeThread.start {
            owner.submit { ran.value = ran.value + 1 }
            submitted.value = 1
        }
        var waited = 0
        while (submitted.value == 0 && waited < WAIT_LIMIT_MILLIS) {
            sleepMillis(5)
            waited += 5
        }

        assertEquals(1, owner.pending, "it waits for the owner rather than running on the caller")
        assertEquals(0, ran.value)
        assertEquals(1, owner.runPending())
        assertEquals(1, ran.value)
    }

    @Test
    fun aTaskThatHandsWorkToItsOwnOwnerDoesItThereAndThen() {
        val owner = Owner("world")
        owner.bindToCurrentThread()
        val order = mutableListOf<String>()

        owner.submit {
            order += "outer"
            owner.submit { order += "inner" }
            order += "after"
        }

        assertEquals(listOf("outer", "inner", "after"), order, "a chain of gameplay does not queue behind itself")
    }

    @Test
    fun whatATaskDelaysBelongsToALaterTurn() {
        val owner = Owner("world")
        owner.bindToCurrentThread()
        var ran = false

        owner.submit { owner.schedule(delayTicks = 0) { ran = true } }

        assertFalse(ran, "asking for a delay is asking for another turn")
        owner.advanceTick()
        owner.runPending()
        assertTrue(ran)
    }

    @Test
    fun anUnclaimedOwnerRunsWhateverItIsHanded() {
        val owner = Owner("world")
        var ran = false

        owner.submit { ran = true }

        assertTrue(ran, "nobody owns it, so the caller may do it")
        assertEquals(0, owner.pending)
    }

    @Test
    fun strictModeRefusesAnAccessFromAnotherThread() {
        val owner = Owner("world")
        val claimed = AtomicInt(0)
        NativeThread.start {
            owner.bindToCurrentThread()
            claimed.value = 1
        }
        var waited = 0
        while (claimed.value == 0 && waited < WAIT_LIMIT_MILLIS) {
            sleepMillis(5)
            waited += 5
        }
        owner.check("The world")

        Ownership.strict = true

        val failure = assertFailsWith<IllegalStateException> { owner.check("The world") }
        assertTrue(failure.message.orEmpty().contains("belongs to 'world'"))
    }

    @Test
    fun strictModeLetsTheOwnerThrough() {
        val owner = Owner("world")
        owner.bindToCurrentThread()
        Ownership.strict = true

        owner.check("The world")
    }

    private companion object {
        const val WAIT_LIMIT_MILLIS = 2000
    }
}
