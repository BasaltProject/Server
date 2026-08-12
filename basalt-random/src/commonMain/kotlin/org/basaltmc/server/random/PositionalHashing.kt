package org.basaltmc.server.random

/**
 * Mirrors vanilla's `net.minecraft.util.Mth.getSeed(int, int, int)`, decompiled from the 26.2
 * server jar. Deliberately reproduces the 32-bit int overflow of `x * 3129871`: this is not
 * "cleaned up" to use longs throughout.
 */
public object PositionalHashing {
    public fun getSeed(
        x: Int,
        y: Int,
        z: Int,
    ): Long {
        var seed = (x * 3129871).toLong() xor z * 116129781L xor y.toLong()
        seed = seed * seed * 42317861L + seed * 11L
        return seed shr 16
    }
}
