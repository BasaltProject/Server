@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.free
import kotlinx.cinterop.nativeHeap
import kotlinx.cinterop.ptr
import platform.posix.pthread_mutex_destroy
import platform.posix.pthread_mutex_init
import platform.posix.pthread_mutex_lock
import platform.posix.pthread_mutex_t
import platform.posix.pthread_mutex_unlock

/**
 * A plain `pthread_mutex_t` wrapper for state shared between [NativeThread]s (Kotlin/Native's
 * stdlib has no blocking lock outside `kotlinx.coroutines.sync.Mutex`, which suspends rather than
 * blocks and doesn't fit this codebase's thread-per-connection model).
 */
public class NativeMutex {
    @PublishedApi
    internal val handle: pthread_mutex_t = nativeHeap.alloc<pthread_mutex_t>()

    init {
        pthread_mutex_init(handle.ptr, null)
    }

    public inline fun <T> withLock(block: () -> T): T {
        pthread_mutex_lock(handle.ptr)
        try {
            return block()
        } finally {
            pthread_mutex_unlock(handle.ptr)
        }
    }

    public fun destroy() {
        pthread_mutex_destroy(handle.ptr)
        nativeHeap.free(handle)
    }
}
