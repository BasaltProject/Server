package org.basaltmc.io

import com.squareup.zstd.ZSTD_e_end
import com.squareup.zstd.getErrorName
import com.squareup.zstd.zstdCompressor
import com.squareup.zstd.zstdDecompressor

/** Thrown for zstd failures (corrupt input, bad compression level). */
public class ZstdException(
    message: String,
) : Exception(message)

/**
 * zstd (RFC 8878), via `com.squareup.zstd:zstd-kmp`: a pure-KMP binding that compiles zstd's own
 * C source per target (no system `libzstd` to install, unlike a hand-rolled cinterop), covering
 * every native target this project builds, including macosX64. This is the compression the Linear
 * region format (`basalt-world`) wraps a whole region in, once per file, unlike Anvil's per-chunk
 * zlib. Only one-shot whole-buffer use is exposed here: the library's own streaming API is lower
 * level (`ZstdCompressor`/`ZstdDecompressor`, raw buffer windows), so this wraps it the same way as
 * this project's other one-shot compression wrappers ([Zlib]).
 */
public object Zstd {
    // ZSTD_cParameter from zstd.h: stable public API values, not generated bindings.
    private const val ZSTD_C_COMPRESSION_LEVEL = 100
    private const val CHUNK_SIZE = 64 * 1024

    public fun compress(
        data: ByteArray,
        level: Int = 3,
    ): ByteArray {
        if (data.isEmpty()) return ByteArray(0)
        return zstdCompressor().use { compressor ->
            checkError(compressor.setParameter(ZSTD_C_COMPRESSION_LEVEL, level))
            val chunks = mutableListOf<ByteArray>()
            var inputStart = 0
            var result: Long
            do {
                val output = ByteArray(CHUNK_SIZE)
                result =
                    compressor.compressStream2(
                        outputByteArray = output,
                        outputEnd = output.size,
                        outputStart = 0,
                        inputByteArray = data,
                        inputEnd = data.size,
                        inputStart = inputStart,
                        mode = ZSTD_e_end,
                    )
                checkError(result)
                inputStart += compressor.inputBytesProcessed
                if (compressor.outputBytesProcessed > 0) chunks += output.copyOf(compressor.outputBytesProcessed)
            } while (inputStart < data.size || result != 0L)
            concatenate(chunks)
        }
    }

    public fun decompress(data: ByteArray): ByteArray {
        if (data.isEmpty()) return ByteArray(0)
        return zstdDecompressor().use { decompressor ->
            val chunks = mutableListOf<ByteArray>()
            var inputStart = 0
            var result: Long
            do {
                val output = ByteArray(CHUNK_SIZE)
                result =
                    decompressor.decompressStream(
                        outputByteArray = output,
                        outputEnd = output.size,
                        outputStart = 0,
                        inputByteArray = data,
                        inputEnd = data.size,
                        inputStart = inputStart,
                    )
                checkError(result)
                inputStart += decompressor.inputBytesProcessed
                if (decompressor.outputBytesProcessed > 0) chunks += output.copyOf(decompressor.outputBytesProcessed)
            } while (inputStart < data.size || result != 0L)
            concatenate(chunks)
        }
    }

    private fun checkError(result: Long) {
        getErrorName(result)?.let { throw ZstdException(it) }
    }

    private fun concatenate(chunks: List<ByteArray>): ByteArray {
        val result = ByteArray(chunks.sumOf { it.size })
        var offset = 0
        for (chunk in chunks) {
            chunk.copyInto(result, offset)
            offset += chunk.size
        }
        return result
    }
}
