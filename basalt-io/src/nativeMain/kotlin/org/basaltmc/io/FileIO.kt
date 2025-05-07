@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.convert
import kotlinx.cinterop.usePinned
import platform.posix.SEEK_END
import platform.posix.SEEK_SET
import platform.posix.fclose
import platform.posix.fopen
import platform.posix.fread
import platform.posix.fseek
import platform.posix.ftell
import platform.posix.fwrite

/** Thrown for file I/O failures (missing parent directory, permissions, disk full). */
public class FileIOException(
    message: String,
) : Exception(message)

/**
 * Plain text file access for config/data files that live outside the plugin/world data model
 * (`basalt.toml`, `eula.txt`, asset caches). `fopen`/`fread`/`fwrite`/`fclose` are real `stdio.h`
 * declarations, not macros, so unlike `select()` they bind identically under `platform.posix` on
 * all three targets, no per-platform `expect`/`actual` needed.
 */
public object FileIO {
    public fun exists(path: String): Boolean {
        val handle = fopen(path, "rb") ?: return false
        fclose(handle)
        return true
    }

    /** Returns `null` if [path] does not exist. */
    public fun readText(path: String): String? {
        val handle = fopen(path, "rb") ?: return null
        try {
            fseek(handle, 0, SEEK_END)
            val size = ftell(handle)
            fseek(handle, 0, SEEK_SET)
            if (size <= 0) return ""

            val buffer = ByteArray(size.toInt())
            val read =
                buffer.usePinned { pinned ->
                    fread(pinned.addressOf(0), 1.convert(), size.convert(), handle)
                }
            return buffer.decodeToString(0, read.toInt())
        } finally {
            fclose(handle)
        }
    }

    public fun writeText(
        path: String,
        content: String,
    ) {
        val handle = fopen(path, "wb") ?: throw FileIOException("failed to open '$path' for writing")
        try {
            val bytes = content.encodeToByteArray()
            if (bytes.isNotEmpty()) {
                bytes.usePinned { pinned ->
                    fwrite(pinned.addressOf(0), 1.convert(), bytes.size.convert(), handle)
                }
            }
        } finally {
            fclose(handle)
        }
    }
}
