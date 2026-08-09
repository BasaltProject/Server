@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.IntVar
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.value
import platform.posix.AF_INET
import platform.posix.SOL_SOCKET
import platform.posix.SO_REUSEADDR
import platform.posix.close
import platform.posix.getpeername
import platform.posix.setsockopt
import platform.posix.sockaddr
import platform.posix.sockaddr_in
import platform.posix.socklen_tVar

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


/**
 * `getpeername` into a [sockaddr_in], which is all this server ever accepts: the listener binds
 * AF_INET, so anything reporting another family is left unanswered rather than misreported.
 */
internal actual fun peerAddress(fd: Long): UInt? =
    memScoped {
        val address = alloc<sockaddr_in>()
        val length = alloc<socklen_tVar>()
        length.value = sizeOf<sockaddr_in>().convert()
        if (getpeername(fd.convert(), address.ptr.reinterpret<sockaddr>(), length.ptr) != 0) {
            return@memScoped null
        }
        if (address.sin_family.toInt() != AF_INET) return@memScoped null
        address.sin_addr.s_addr
    }
