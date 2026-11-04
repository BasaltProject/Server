// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record UniformInt(int minInclusive, int maxInclusive) implements IntProvider {
    public static final MapCodec<UniformInt> MAP_CODEC = RecordCodecBuilder.<UniformInt>mapCodec(i -> i.group(
        Codec.INT.fieldOf("min_inclusive").forGetter(UniformInt::minInclusive),
        Codec.INT.fieldOf("max_inclusive").forGetter(UniformInt::maxInclusive)).apply(i, UniformInt::new)).validate(value -> value.maxInclusive < value.minInclusive
        ? DataResult.error(() -> "Max must be at least min, min_inclusive: " + value.minInclusive + ", max_inclusive: " + value.maxInclusive)
        : DataResult.success(value));

    public static UniformInt of(int minInclusive, int maxInclusive) {
        return new UniformInt(minInclusive, maxInclusive);
    }

    public int sample(RandomSource random) {
        return Mth.randomBetweenInclusive(random, minInclusive, maxInclusive);
    }

    public MapCodec<UniformInt> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return "[" + minInclusive + "-" + maxInclusive + "]";
    }
}
