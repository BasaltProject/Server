// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
public class LinearCongruentialGenerator {
    private static final long kMultiplier = 6364136223846793005L;
    private static final long kIncrement = 1442695040888963407L;
    public static long next(long seed, long salt) {
        seed *= seed * kMultiplier + kIncrement;
        return seed + salt;
    }
}
