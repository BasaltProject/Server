package org.basaltmc.server.random

import org.basaltmc.wire.common.hash.Md5

/** Mirrors vanilla's `net.minecraft.world.level.levelgen.RandomSupport`. */
public object RandomSupport {
    public const val GOLDEN_RATIO_64: Long = -7046029254386353131L
    public const val SILVER_RATIO_64: Long = 7640891576956012809L

    public fun mixStafford13(input: Long): Long {
        var z = input
        z = (z xor (z ushr 30)) * -4658895280553007687L
        z = (z xor (z ushr 27)) * -7723592293110705685L
        return z xor (z ushr 31)
    }

    public fun upgradeSeedTo128bitUnmixed(legacySeed: Long): Seed128bit {
        val lowBits = legacySeed xor SILVER_RATIO_64
        val highBits = lowBits + GOLDEN_RATIO_64
        return Seed128bit(lowBits, highBits)
    }

    public fun upgradeSeedTo128bit(legacySeed: Long): Seed128bit = upgradeSeedTo128bitUnmixed(legacySeed).mixed()

    /** Reproduces Guava's `Longs.fromBytes`: big-endian packing of 8 bytes into a [Long]. */
    private fun bytesToLongBigEndian(
        bytes: ByteArray,
        offset: Int,
    ): Long {
        var result = 0L
        for (i in 0 until 8) {
            result = (result shl 8) or (bytes[offset + i].toLong() and 0xFFL)
        }
        return result
    }

    public fun seedFromHashOf(input: String): Seed128bit {
        val hash = Md5.digest(input.encodeToByteArray())
        val hashLo = bytesToLongBigEndian(hash, 0)
        val hashHi = bytesToLongBigEndian(hash, 8)
        return Seed128bit(hashLo, hashHi)
    }
}

/** A 128-bit seed as a pair of longs, mirroring `RandomSupport.Seed128bit`. */
public data class Seed128bit(
    val seedLo: Long,
    val seedHi: Long,
) {
    public fun xor(
        lo: Long,
        hi: Long,
    ): Seed128bit = Seed128bit(seedLo xor lo, seedHi xor hi)

    public fun xor(other: Seed128bit): Seed128bit = xor(other.seedLo, other.seedHi)

    public fun mixed(): Seed128bit =
        Seed128bit(RandomSupport.mixStafford13(seedLo), RandomSupport.mixStafford13(seedHi))
}
