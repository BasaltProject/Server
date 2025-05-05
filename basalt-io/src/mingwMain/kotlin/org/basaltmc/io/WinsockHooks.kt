@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import platform.posix.WSADATA
import platform.posix.WSAStartup
import platform.posix.closesocket

/**
 * Winsock, unlike POSIX sockets, requires `WSAStartup` before any other socket call. Called
 * unconditionally on every [TcpListener.bind]/[TcpConnection.connect] rather than guarded behind
 * an initialize-once flag: `WSAStartup` is refcounted by the OS and safe to call repeatedly, and
 * there is nowhere in this codebase yet that runs at process shutdown to pair it with
 * `WSACleanup`.
 *
 * Requests Winsock version 2.2: `0x0202`, i.e. the raw value of the `MAKEWORD(2, 2)` macro (macros
 * aren't callable from Kotlin via cinterop, only real declarations are, so this is computed by
 * hand instead of referencing the macro).
 *
 * Unverified: this repo builds only macOS targets locally, and CI is not currently running.
 * `WSAStartup`/`WSADATA`/`closesocket` are assumed to live under `platform.posix` on the mingw
 * target because the rest of the Winsock-flavored socket API (`socket`/`bind`/
 * `connect`/`send`/`recv`/`setsockopt`/`listen`) already does, per the original Windows CI compile
 * errors (type mismatches, not unresolved-reference errors, for those). If that assumption is
 * wrong, the fix is almost certainly just swapping this import for `platform.windows`.
 */
internal actual fun ensureSocketsInitialized() {
    memScoped {
        val wsaData = alloc<WSADATA>()
        WSAStartup(0x0202.convert(), wsaData.ptr)
    }
}

internal actual fun closeSocket(fd: Long) {
    closesocket(fd.convert())
}
