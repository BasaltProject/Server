// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.util.RandomSource;

public class MarsagliaPolarGaussian {
    public final RandomSource randomSource;
    private double nextNextGaussian;
    private boolean haveNextNextGaussian;

    public MarsagliaPolarGaussian(RandomSource randomSource) {
        this.randomSource = randomSource;
    }

    public void reset() {
        haveNextNextGaussian = false;
    }

    public double nextGaussian() {
        if (haveNextNextGaussian) {
            haveNextNextGaussian = false;
            return nextNextGaussian;
        }
        double x;
        double y;
        double radiusSquared;
        do {
            x = 2.0 * randomSource.nextDouble() - 1.0;
            y = 2.0 * randomSource.nextDouble() - 1.0;
            radiusSquared = x * x + y * y;
        } while (radiusSquared >= 1.0 || radiusSquared == 0.0);
        double multiplier = Math.sqrt(-2.0 * Math.log(radiusSquared) / radiusSquared);
        nextNextGaussian = y * multiplier;
        haveNextNextGaussian = true;
        return x * multiplier;
    }
}
