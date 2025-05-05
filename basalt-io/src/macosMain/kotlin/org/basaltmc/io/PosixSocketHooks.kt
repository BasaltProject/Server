@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.convert
import platform.posix.close

internal actual fun ensureSocketsInitialized() {
    // no-op: POSIX sockets need no process-wide setup.
}

internal actual fun closeSocket(fd: Long) {
    close(fd.convert())
}
