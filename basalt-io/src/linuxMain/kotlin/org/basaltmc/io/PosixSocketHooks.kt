@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.IntVar
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.value
import platform.posix.SOL_SOCKET
import platform.posix.SO_REUSEADDR
import platform.posix.close
import platform.posix.setsockopt
import platform.posix.sockaddr_in

internal actual fun ensureSocketsInitialized() {
    // no-op: POSIX sockets need no process-wide setup.
}

internal actual fun closeSocket(fd: Long) {
    close(fd.convert())
}

internal actual fun enableAddressReuse(fd: Long) {
    memScoped {
        val reuse = alloc<IntVar>()
        reuse.value = 1
        setsockopt(fd.convert(), SOL_SOCKET, SO_REUSEADDR, reuse.ptr, sizeOf<IntVar>().convert())
    }
}

internal actual fun sockaddr_in.setAddress(value: UInt) {
    sin_addr.s_addr = value
}
