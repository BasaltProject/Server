package org.basaltmc.auth

/**
 * Formats a raw SHA-1 digest the way vanilla's `new BigInteger(digest).toString(16)` does:
 * the big-endian bytes are read as a signed two's-complement integer, then rendered as lowercase
 * hex with no leading zero digits (a `-` prefix if negative, none otherwise). This is the
 * "server hash" sent to Mojang's session server for online-mode join verification.
 *
 * // parity: `net.minecraft.server.network.ServerLoginPacketListenerImpl.handleKey`, which
 * computes `new BigInteger(Crypt.digestData(serverId, publicKey, secretKey)).toString(16)`.
 * Verified against real `java.math.BigInteger` output for both signs and both leading-zero-byte
 * and leading-zero-nibble edge cases (see [SessionHashTest]); this is not derived from memory of
 * the wiki.vg algorithm description.
 */
public object SessionHash {
    public fun format(digest: ByteArray): String {
        require(digest.isNotEmpty()) { "digest must not be empty" }
        val negative = (digest[0].toInt() and 0x80) != 0
        val magnitude = if (negative) twosComplementNegate(digest) else digest
        val hex = toHexWithoutLeadingZeros(magnitude)
        return if (negative) "-$hex" else hex
    }

    private fun twosComplementNegate(bytes: ByteArray): ByteArray {
        val result = ByteArray(bytes.size)
        var carry = 1
        for (i in bytes.indices.reversed()) {
            val inverted = (bytes[i].toInt() xor 0xFF) and 0xFF
            val sum = inverted + carry
            result[i] = sum.toByte()
            carry = sum ushr 8
        }
        return result
    }

    private fun toHexWithoutLeadingZeros(bytes: ByteArray): String {
        val hex = StringBuilder()
        var leading = true
        for (byte in bytes) {
            val value = byte.toInt() and 0xFF
            if (leading && value == 0) continue
            hex.append(if (leading) value.toString(16) else value.toString(16).padStart(2, '0'))
            leading = false
        }
        return if (hex.isEmpty()) "0" else hex.toString()
    }
}
