package org.basaltmc.server.runtime.net

import org.basaltmc.wire.io.NativeMutex

/**
 * The frames waiting to go out on one connection.
 *
 * Sockets here block, so whoever writes to a client that has stopped draining is parked until it
 * drains. That thread must never be the tick thread: a single slow client would hold up the world.
 * The tick thread hands its frames here instead, and the connection's own outbound thread is the
 * only one that touches the socket.
 *
 * Order is the queue's order, which is why every frame goes through it once it is open: a writer
 * that bypassed it would interleave with what is waiting and the client would read them swapped.
 */
public class SessionOutbox(
    /** Past this many bytes waiting, the client is not draining and the connection has to go. */
    private val limitBytes: Long = DEFAULT_LIMIT_BYTES,
) {
    private val mutex = NativeMutex()
    private var queue = ArrayDeque<ByteArray>()
    private var pending = 0L
    private var open = false
    private var overflowed = false

    /** Whether frames are being queued rather than written straight to the socket. */
    public val isOpen: Boolean get() = mutex.withLock { open }

    /** How many bytes are waiting, for the timings and for the overflow check. */
    public val pendingBytes: Long get() = mutex.withLock { pending }

    /** Whether the client stopped draining and the connection has to go. */
    public val hasOverflowed: Boolean get() = mutex.withLock { overflowed }

    /** From here on frames are queued. Called when the outbound thread starts, and only then. */
    public fun open() {
        mutex.withLock { open = true }
    }

    /**
     * Queues one frame. Answers false when the outbox is not open yet, so the caller writes it
     * straight to the socket as it did before there was an outbox (login and configuration).
     */
    public fun offer(frame: ByteArray): Boolean =
        mutex.withLock {
            if (!open) return@withLock false
            if (pending + frame.size > limitBytes) {
                // Dropping frames would desynchronize the client anyway, so the honest answer is to
                // stop queueing and let the connection be closed.
                overflowed = true
                return@withLock true
            }
            queue.addLast(frame)
            pending += frame.size
            true
        }

    /**
     * Takes everything waiting, in order. The caller writes it and is the only one that does, so
     * the cipher stays in send order even though the frames came from several threads.
     */
    public fun drain(): List<ByteArray> =
        mutex.withLock {
            if (queue.isEmpty()) return@withLock emptyList()
            val taken = queue
            queue = ArrayDeque()
            pending = 0
            taken
        }

    /** Nothing else goes out: the connection is gone. */
    public fun close() {
        mutex.withLock {
            open = false
            queue = ArrayDeque()
            pending = 0
        }
    }

    public companion object {
        /** Eight megabytes of unsent frames is a client that is never coming back. */
        public const val DEFAULT_LIMIT_BYTES: Long = 8L * 1024 * 1024
    }
}
