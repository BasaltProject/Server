package org.basaltmc.server.plugins

/** Builds a ZIP in memory, so no test needs a binary fixture nobody can review. */
object TestZip {
    /** A ZIP with stored (uncompressed) entries, which is all this needs to exercise the reader. */
    fun zipOf(vararg entries: Pair<String, String>): ByteArray {
        val out = mutableListOf<Byte>()
        val offsets = mutableListOf<Int>()
        for ((name, content) in entries) {
            offsets += out.size
            val nameBytes = name.encodeToByteArray()
            val data = content.encodeToByteArray()
            out.u32(0x04034b50)
            out.u16(20)
            out.u16(0)
            out.u16(0) // stored
            out.u16(0)
            out.u16(0)
            out.u32(0) // crc, which this reader does not check
            out.u32(data.size)
            out.u32(data.size)
            out.u16(nameBytes.size)
            out.u16(0)
            out.addAll(nameBytes.toList())
            out.addAll(data.toList())
        }
        val directoryStart = out.size
        for ((index, entry) in entries.withIndex()) {
            val nameBytes = entry.first.encodeToByteArray()
            val size = entry.second.encodeToByteArray().size
            out.u32(0x02014b50)
            out.u16(20)
            out.u16(20)
            out.u16(0)
            out.u16(0)
            out.u16(0)
            out.u16(0)
            out.u32(0)
            out.u32(size)
            out.u32(size)
            out.u16(nameBytes.size)
            out.u16(0)
            out.u16(0)
            out.u16(0)
            out.u16(0)
            out.u32(0)
            out.u32(offsets[index])
            out.addAll(nameBytes.toList())
        }
        val directorySize = out.size - directoryStart
        out.u32(0x06054b50)
        out.u16(0)
        out.u16(0)
        out.u16(entries.size)
        out.u16(entries.size)
        out.u32(directorySize)
        out.u32(directoryStart)
        out.u16(0)
        return out.toByteArray()
    }

    private fun MutableList<Byte>.u16(value: Int) {
        add((value and 0xFF).toByte())
        add(((value shr 8) and 0xFF).toByte())
    }

    private fun MutableList<Byte>.u32(value: Int) {
        u16(value and 0xFFFF)
        u16((value shr 16) and 0xFFFF)
    }

    private fun MutableList<Byte>.u32(value: Long) = u32(value.toInt())
}
