// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record VeryBiasedToBottomInt(int minInclusive, int maxInclusive) implements IntProvider {
    public static final MapCodec<VeryBiasedToBottomInt> MAP_CODEC = RecordCodecBuilder.<VeryBiasedToBottomInt>mapCodec(i -> i.group(
        Codec.INT.fieldOf("min_inclusive").forGetter(VeryBiasedToBottomInt::minInclusive),
        Codec.INT.fieldOf("max_inclusive").forGetter(VeryBiasedToBottomInt::maxInclusive)).apply(i, VeryBiasedToBottomInt::new)).validate(value -> value.maxInclusive < value.minInclusive
        ? DataResult.error(() -> "Max must be at least min, min_inclusive: " + value.minInclusive + ", max_inclusive: " + value.maxInclusive)
        : DataResult.success(value));

    public static VeryBiasedToBottomInt of(int minInclusive, int maxInclusive) {
        return new VeryBiasedToBottomInt(minInclusive, maxInclusive);
    }

    public int sample(RandomSource random) {
        return minInclusive + random.nextInt(random.nextInt(random.nextInt(maxInclusive - minInclusive + 1) + 1) + 1);
    }

    public MapCodec<VeryBiasedToBottomInt> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return "[" + minInclusive + "-" + maxInclusive + "]";
    }
}
