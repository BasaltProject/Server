package org.basaltmc.random

/** Mirrors vanilla's `PositionalRandomFactory`: derives deterministic per-position/per-name RNGs. */
public interface PositionalRandomFactory {
    public fun at(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom

    public fun fromHashOf(name: String): BasaltRandom

    public fun fromSeed(seed: Long): BasaltRandom
}
