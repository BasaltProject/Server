package org.basaltmc.server.runtime.sched

import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The task API a region runs on: delays, the end of a turn, cancellation and what a closed one does. */
class TaskApiTest {
    /** An owner claimed by another thread, so submitting from this one queues instead of running. */
    private fun ownedElsewhere(): Owner {
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
        return owner
    }

    @Test
    fun aDelayedTaskWaitsItsTurns() {
        val owner = ownedElsewhere()
        var ran = 0

        owner.schedule(delayTicks = 2) { ran++ }

        owner.advanceTick()
        owner.bindToCurrentThread()
        assertEquals(0, owner.runPending(), "two turns still to wait")
        owner.advanceTick()
        assertEquals(0, owner.runPending(), "one to go")
        owner.advanceTick()
        assertEquals(1, owner.runPending())
        assertEquals(1, ran)
    }

    @Test
    fun zeroTicksIsTheNextTurn() {
        val owner = ownedElsewhere()
        var ran = false

        owner.schedule(delayTicks = 0) { ran = true }
        owner.advanceTick()
        owner.bindToCurrentThread()
        owner.runPending()

        assertTrue(ran)
    }

    @Test
    fun whatAsksForTheEndOfTheTurnRunsThere() {
        val owner = ownedElsewhere()
        val order = mutableListOf<String>()

        owner.submit { order += "start" }
        owner.scheduleAtEnd { order += "end" }

        owner.bindToCurrentThread()

        owner.runPending()
        assertEquals(listOf("start"), order, "the end of the turn has not come yet")
        owner.runEndOfTick()
        assertEquals(listOf("start", "end"), order)
    }

    @Test
    fun aCancelledTaskNeverRuns() {
        val owner = ownedElsewhere()
        var ran = false

        val handle = owner.submit { ran = true }
        handle.cancel()
        owner.bindToCurrentThread()
        owner.runPending()

        assertFalse(ran)
        assertTrue(handle.isCancelled)
    }

    @Test
    fun aCancelledDelayIsForgottenRatherThanCounted() {
        val owner = ownedElsewhere()

        val handle = owner.schedule(delayTicks = 5) { }
        handle.cancel()
        owner.advanceTick()

        assertEquals(0, owner.pending, "it is dropped on the next turn rather than waiting five")
    }

    @Test
    fun aClosedOwnerRunsNothingAndSaysSo() {
        val owner = ownedElsewhere()
        var ran = false
        val queued = owner.submit { ran = true }
        val delayed = owner.schedule(delayTicks = 1) { ran = true }

        owner.close()

        assertTrue(queued.isCancelled, "what was waiting is told it is not happening")
        assertTrue(delayed.isCancelled)
        assertTrue(owner.submit { ran = true }.isCancelled, "and nothing new is taken")
        owner.advanceTick()
        owner.bindToCurrentThread()
        owner.runPending()
        owner.runEndOfTick()
        assertFalse(ran)
        assertTrue(owner.isClosed)
    }

    @Test
    fun aTurnCostsNoQueueOfItsOwn() {
        val owner = ownedElsewhere()
        var ran = 0

        repeat(3) {
            owner.submit { ran++ }
            owner.bindToCurrentThread()
            owner.runPending()
        }

        assertEquals(3, ran, "the two buffers are swapped rather than allocated, and still run once each")
        assertEquals(0, owner.pending)
    }

    private companion object {
        const val WAIT_LIMIT_MILLIS = 2000
    }
}
