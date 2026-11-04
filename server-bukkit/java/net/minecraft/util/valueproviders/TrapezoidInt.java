// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record TrapezoidInt(int minInclusive, int maxInclusive, int plateau) implements IntProvider {
    public static final MapCodec<TrapezoidInt> MAP_CODEC = RecordCodecBuilder.<TrapezoidInt>mapCodec(i -> i.group(
        Codec.INT.fieldOf("min").forGetter(TrapezoidInt::minInclusive), Codec.INT.fieldOf("max").forGetter(TrapezoidInt::maxInclusive),
        Codec.INT.fieldOf("plateau").forGetter(TrapezoidInt::plateau)).apply(i, TrapezoidInt::new)).validate(value -> {
            if (value.maxInclusive < value.minInclusive) {
                return DataResult.error(() -> "Max must be larger than min: [" + value.minInclusive + ", " + value.maxInclusive + "]");
            }
            return value.plateau > value.maxInclusive - value.minInclusive
                ? DataResult.error(() -> "Plateau can at most be the full span: [" + value.minInclusive + ", " + value.maxInclusive + "]")
                : DataResult.success(value);
        });

    public static TrapezoidInt of(int min, int max, int plateau) {
        return new TrapezoidInt(min, max, plateau);
    }

    public static IntProvider triangle(int range) {
        return of(-range, range, 0);
    }

    public int sample(RandomSource random) {
        if (plateau == 0 && maxInclusive == -minInclusive) {
            return random.nextInt(maxInclusive + 1) - random.nextInt(maxInclusive + 1);
        }
        int range = maxInclusive - minInclusive;
        if (plateau == range) {
            return Mth.randomBetweenInclusive(random, minInclusive, maxInclusive);
        }
        int plateauStart = (range - plateau) / 2;
        int plateauEnd = range - plateauStart;
        return minInclusive + Mth.randomBetweenInclusive(random, 0, plateauEnd) + Mth.randomBetweenInclusive(random, 0, plateauStart);
    }

    public MapCodec<TrapezoidInt> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return "trapezoid(" + plateau + ") in [" + minInclusive + "-" + maxInclusive + "]";
    }
}
