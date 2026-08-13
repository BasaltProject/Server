package org.basaltmc.server.runtime

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.io.TcpListener
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.test.fail

/**
 * The guard itself, because an untested one is worth nothing: a read that will never be answered
 * has to end the test with a message, not park the run on `recv` until somebody notices.
 */
class ReadStallWatchdogTest {
    @AfterTest
    fun tearDown() {
        ReadStallWatchdog.deadlineMillis = DEFAULT_DEADLINE_MILLIS
        ReadStallWatchdog.fired.value = 0
    }

    @Test
    fun aReadThatIsNeverAnsweredEndsWithAMessage() {
        var bound: TcpListener? = null
        var port = FIRST_PORT
        while (bound == null && port < FIRST_PORT + PORT_ATTEMPTS) {
            bound = runCatching { TcpListener.bind(port) }.getOrNull()
            if (bound == null) port++
        }
        val listener = requireNotNull(bound) { "no free port in the test range" }
        val accepted = AtomicInt(0)
        // Accepts and then says nothing at all, which is what a stalled server looks like.
        NativeThread.start {
            val server = listener.accept()
            accepted.incrementAndGet()
            while (accepted.value == 1) sleepMillis(POLL_MILLIS)
            runCatching { server.close() }
        }

        ReadStallWatchdog.deadlineMillis = SHORT_DEADLINE_MILLIS
        val connection = TcpConnection.connect("127.0.0.1", port)
        try {
            readFrame(connection, ByteBuffer(), compressionEnabled = false)
            fail("a read with no answer should not have returned a frame")
        } catch (e: IllegalStateException) {
            assertTrue(
                "read deadline" in (e.message ?: ""),
                "the failure did not name the watchdog: ${e.message}",
            )
        } finally {
            accepted.value = 2
            runCatching { connection.close() }
            runCatching { listener.close() }
        }
    }

    private companion object {
        const val FIRST_PORT = 18990
        const val PORT_ATTEMPTS = 20
        const val DEFAULT_DEADLINE_MILLIS = 60_000L
        const val SHORT_DEADLINE_MILLIS = 1_000L
        const val POLL_MILLIS = 50L
    }
}
