package org.basaltmc.server.runtime.net

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The queue that keeps the tick thread off a slow client's socket. */
class SessionOutboxTest {
    @Test
    fun beforeItIsOpenNothingIsQueued() {
        val outbox = SessionOutbox()
        assertFalse(outbox.offer(byteArrayOf(1, 2, 3)), "the caller writes it straight out instead")
        assertEquals(0, outbox.pendingBytes)
    }

    @Test
    fun onceOpenTheFramesComeBackInOrder() {
        val outbox = SessionOutbox()
        outbox.open()
        assertTrue(outbox.offer(byteArrayOf(1)))
        assertTrue(outbox.offer(byteArrayOf(2, 2)))
        assertTrue(outbox.offer(byteArrayOf(3, 3, 3)))
        assertEquals(6, outbox.pendingBytes)

        val drained = outbox.drain()

        assertEquals(3, drained.size)
        assertContentEquals(byteArrayOf(1), drained[0])
        assertContentEquals(byteArrayOf(3, 3, 3), drained[2])
        assertEquals(0, outbox.pendingBytes, "draining takes everything")
        assertEquals(0, outbox.drain().size)
    }

    @Test
    fun aClientThatStopsDrainingOverflowsInsteadOfGrowingForEver() {
        val outbox = SessionOutbox(limitBytes = 8)
        outbox.open()
        assertTrue(outbox.offer(ByteArray(6)))
        assertFalse(outbox.hasOverflowed)

        outbox.offer(ByteArray(6))

        assertTrue(outbox.hasOverflowed, "past the limit the connection has to go")
        assertEquals(6, outbox.pendingBytes, "and nothing more is queued")
    }

    @Test
    fun closingThrowsAwayWhatNobodyWillRead() {
        val outbox = SessionOutbox()
        outbox.open()
        outbox.offer(ByteArray(16))

        outbox.close()

        assertEquals(0, outbox.pendingBytes)
        assertFalse(outbox.isOpen)
        assertFalse(outbox.offer(ByteArray(1)), "a closed outbox queues nothing")
    }
}
