package org.basaltmc.server.player

import org.basaltmc.io.NativeMutex
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The set of currently connected players, shared across every [org.basaltmc.server.ClientSession]'s
 * own [org.basaltmc.io.NativeThread]. A plain instance passed in by [org.basaltmc.server.BasaltServer],
 * not a singleton: callers that need it get it via their constructor, not a static accessor.
 */
@OptIn(ExperimentalUuidApi::class)
public class PlayerRegistry {
    private val mutex = NativeMutex()
    private val sessions = LinkedHashMap<Uuid, PlayerSession>()
    private var nextEntityId = 1

    public fun allocateEntityId(): Int = mutex.withLock { nextEntityId++ }

    public fun add(session: PlayerSession) {
        mutex.withLock { sessions[session.uuid] = session }
    }

    public fun remove(uuid: Uuid) {
        mutex.withLock { sessions.remove(uuid) }
    }

    public fun get(uuid: Uuid): PlayerSession? = mutex.withLock { sessions[uuid] }

    /** A point-in-time copy: safe to iterate without holding the lock (and while it may itself change concurrently). */
    public fun snapshot(): List<PlayerSession> = mutex.withLock { sessions.values.toList() }
}
