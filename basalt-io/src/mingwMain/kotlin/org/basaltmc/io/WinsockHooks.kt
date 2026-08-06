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
 * Confirmed on real Windows CI: `WSAStartup`/`WSADATA`/`closesocket` do live under `platform.posix`
 * on the mingw target, alongside the rest of the Winsock-flavored socket API.
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

/**
 * Deliberately not implemented: `SO_REUSEADDR` needs `optval` as a raw pointer, but Winsock's
 * `setsockopt` declares it `const char*`, which cinterop binds as a Kotlin `String`, not a
 * pointer. See [enableAddressReuse]'s KDoc in `TcpSocket.kt` for why that's a no-op here rather
 * than a guessed workaround.
 */
internal actual fun enableAddressReuse(fd: Long) {
    // intentionally a no-op on Windows; see the expect declaration's KDoc
}
