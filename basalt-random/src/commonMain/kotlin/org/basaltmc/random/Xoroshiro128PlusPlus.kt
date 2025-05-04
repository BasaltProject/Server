package org.basaltmc.random

/** Mirrors vanilla's `net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus`. */
public class Xoroshiro128PlusPlus(
    seedLo: Long,
    seedHi: Long,
) {
    private var seedLo: Long
    private var seedHi: Long

    init {
        if ((seedLo or seedHi) == 0L) {
            this.seedLo = RandomSupport.GOLDEN_RATIO_64
            this.seedHi = RandomSupport.SILVER_RATIO_64
        } else {
            this.seedLo = seedLo
            this.seedHi = seedHi
        }
    }

    public constructor(seed: Seed128bit) : this(seed.seedLo, seed.seedHi)

    public fun nextLong(): Long {
        val s0 = seedLo
        var s1 = seedHi
        val result = (s0 + s1).rotateLeft(17) + s0
        s1 = s1 xor s0
        seedLo = s0.rotateLeft(49) xor s1 xor (s1 shl 21)
        seedHi = s1.rotateLeft(28)
        return result
    }
}
