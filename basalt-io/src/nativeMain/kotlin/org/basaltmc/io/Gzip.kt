@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.sizeOf
import kotlinx.cinterop.usePinned
import platform.zlib.ZLIB_VERSION
import platform.zlib.Z_NO_FLUSH
import platform.zlib.Z_OK
import platform.zlib.Z_STREAM_END
import platform.zlib.inflate
import platform.zlib.inflateEnd
import platform.zlib.inflateInit2_
import platform.zlib.z_stream

/**
 * gzip (RFC 1952) decompression, the wrapper saved `.nbt` structure files (and
 * eventually world saves) actually use, distinct from [Zlib]'s RFC 1950 and [RawInflate]'s bare
 * RFC 1951. `windowBits = 31` (`15 + 16`) tells zlib to expect and validate a real gzip header
 * itself, the same `inflateInit2_` macro-expansion trick [RawInflate] already uses, rather than
 * this project parsing the gzip header/trailer by hand.
 *
 * The uncompressed size doesn't need to be known ahead of time: RFC 1952's own trailer stores it
 * (`ISIZE`, the last 4 bytes, little-endian, mod 2^32), which is read directly instead of asking
 * the caller for it.
 */
public object Gzip {
    private const val GZIP_WINDOW_BITS = 31

    public fun decompress(data: ByteArray): ByteArray {
        require(data.size >= 18) { "Input too small to be a valid gzip stream (${data.size} bytes)" }
        val trailerOffset = data.size - 4
        val uncompressedSize =
            (data[trailerOffset].toInt() and 0xFF) or
                ((data[trailerOffset + 1].toInt() and 0xFF) shl 8) or
                ((data[trailerOffset + 2].toInt() and 0xFF) shl 16) or
                ((data[trailerOffset + 3].toInt() and 0xFF) shl 24)
        if (uncompressedSize == 0) return ByteArray(0)

        val dest = UByteArray(uncompressedSize)
        val source = data.toUByteArray()
        return memScoped {
            val stream = alloc<z_stream>()
            val initResult = inflateInit2_(stream.ptr, GZIP_WINDOW_BITS, ZLIB_VERSION, sizeOf<z_stream>().convert())
            if (initResult != Z_OK) throw ZlibException(initResult)

            try {
                source.usePinned { srcPinned ->
                    dest.usePinned { destPinned ->
                        stream.next_in = srcPinned.addressOf(0)
                        stream.avail_in = data.size.convert()
                        stream.next_out = destPinned.addressOf(0)
                        stream.avail_out = uncompressedSize.convert()

                        val result = inflate(stream.ptr, Z_NO_FLUSH)
                        if (result != Z_STREAM_END && result != Z_OK) throw ZlibException(result)
                    }
                }
            } finally {
                inflateEnd(stream.ptr)
            }
            dest.toByteArray()
        }
    }
}
