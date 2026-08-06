@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.IntVar
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.usePinned
import kotlinx.cinterop.value
import platform.posix.AF_INET
import platform.posix.INADDR_ANY
import platform.posix.SOCK_STREAM
import platform.posix.SOL_SOCKET
import platform.posix.SO_REUSEADDR
import platform.posix.accept
import platform.posix.bind
import platform.posix.close
import platform.posix.connect
import platform.posix.listen
import platform.posix.recv
import platform.posix.send
import platform.posix.setsockopt
import platform.posix.sockaddr
import platform.posix.sockaddr_in
import platform.posix.socket

/**
 * Thrown for any POSIX socket syscall failure. Wraps the raw `errno`-adjacent return code rather
 * than trying to translate it: platform error codes differ enough across macOS/Linux/Windows that
 * a shared translation layer belongs to a later pass once all three are actually buildable here.
 */
public class SocketException(
    message: String,
) : Exception(message)

/**
 * Sets `sin_family` (and, on BSD-family systems, `sin_len`) for a fresh [sockaddr_in]. Split out
 * as `expect`/`actual` because the struct genuinely differs at the ABI level, not just in the
 * bundled Kotlin binding: BSD/Darwin's `sockaddr_in` has a `u_char sin_len` field before an
 * `u_char sin_family` (4.4BSD's variable-length-record convention), while Linux's has no length
 * field at all and `sin_family` alone is a 16-bit `sa_family_t`. `nativeMain` code has to call
 * through this rather than assign the fields directly, or it only compiles for whichever family
 * was used to write it.
 */
internal expect fun sockaddr_in.setAddressFamily()

/**
 * Blocking IPv4 TCP sockets over `platform.posix`, which Kotlin/Native bundles for every POSIX
 * target (no custom cinterop needed). This is a thread-per-connection building block, not an
 * event loop: `epoll`/`kqueue`/IOCP-based async I/O is deliberately out of scope for this pass
 * and is a tracked follow-up.
 *
 * Only literal IPv4 addresses are supported (no DNS resolution) and only little-endian hosts
 * (true of every target Basalt builds for: ARM64 and x64). Windows (mingw) needs its own
 * winsock-based implementation (`closesocket`, `WSAStartup`) and is not covered here yet.
 */
public class TcpListener private constructor(
    private val fd: Int,
) {
    public fun accept(): TcpConnection {
        val clientFd = accept(fd, null, null)
        if (clientFd < 0) throw SocketException("accept() failed")
        return TcpConnection(clientFd)
    }

    public fun close() {
        close(fd)
    }

    public companion object {
        public fun bind(
            port: Int,
            backlog: Int = 128,
        ): TcpListener {
            val fd = socket(AF_INET, SOCK_STREAM, 0)
            if (fd < 0) throw SocketException("socket() failed")

            memScoped {
                val reuse = alloc<IntVar>()
                reuse.value = 1
                setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reuse.ptr, sizeOf<IntVar>().convert())

                val address = alloc<sockaddr_in>()
                address.setAddressFamily()
                address.sin_port = hostToNetworkShort(port)
                address.sin_addr.s_addr = INADDR_ANY

                if (bind(fd, address.ptr.reinterpret<sockaddr>(), sizeOf<sockaddr_in>().convert()) != 0) {
                    close(fd)
                    throw SocketException("bind() failed on port $port")
                }
            }

            if (listen(fd, backlog) != 0) {
                close(fd)
                throw SocketException("listen() failed")
            }
            return TcpListener(fd)
        }
    }
}

public class TcpConnection internal constructor(
    private val fd: Int,
) {
    public fun read(
        buffer: ByteArray,
        offset: Int = 0,
        length: Int = buffer.size - offset,
    ): Int {
        val received =
            buffer.usePinned { pinned ->
                recv(fd, pinned.addressOf(offset), length.convert(), 0)
            }
        if (received < 0) throw SocketException("recv() failed")
        return received.toInt()
    }

    public fun write(
        buffer: ByteArray,
        offset: Int = 0,
        length: Int = buffer.size - offset,
    ) {
        var written = 0
        while (written < length) {
            val sent =
                buffer.usePinned { pinned ->
                    send(fd, pinned.addressOf(offset + written), (length - written).convert(), 0)
                }
            if (sent < 0) throw SocketException("send() failed")
            written += sent.toInt()
        }
    }

    public fun close() {
        close(fd)
    }

    public companion object {
        public fun connect(
            host: String,
            port: Int,
        ): TcpConnection {
            val fd = socket(AF_INET, SOCK_STREAM, 0)
            if (fd < 0) throw SocketException("socket() failed")

            memScoped {
                val address = alloc<sockaddr_in>()
                address.setAddressFamily()
                address.sin_port = hostToNetworkShort(port)
                address.sin_addr.s_addr = parseIpv4NetworkOrder(host)

                if (connect(fd, address.ptr.reinterpret<sockaddr>(), sizeOf<sockaddr_in>().convert()) != 0) {
                    close(fd)
                    throw SocketException("connect() failed to $host:$port")
                }
            }
            return TcpConnection(fd)
        }
    }
}

/** Byte-swaps [value] for a little-endian host so it lands in the field in network byte order. */
private fun hostToNetworkShort(value: Int): UShort = (((value and 0xFF) shl 8) or ((value ushr 8) and 0xFF)).toUShort()

/** Parses a literal IPv4 address (e.g. "127.0.0.1") into the little-endian-host in_addr value. */
private fun parseIpv4NetworkOrder(address: String): UInt {
    val octets = address.split(".").map { it.toUInt() }
    require(octets.size == 4 && octets.all { it <= 255u }) { "Not a literal IPv4 address: $address" }
    return octets[0] or (octets[1] shl 8) or (octets[2] shl 16) or (octets[3] shl 24)
}
