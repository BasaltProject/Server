// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record ClampedInt(IntProvider source, int minInclusive, int maxInclusive) implements IntProvider {
    public static final MapCodec<ClampedInt> MAP_CODEC = RecordCodecBuilder.<ClampedInt>mapCodec(i -> i.group(
        IntProviders.CODEC.fieldOf("source").forGetter(ClampedInt::source), Codec.INT.fieldOf("min_inclusive").forGetter(ClampedInt::minInclusive),
        Codec.INT.fieldOf("max_inclusive").forGetter(ClampedInt::maxInclusive)).apply(i, ClampedInt::new)).validate(value -> value.maxInclusive < value.minInclusive
        ? DataResult.error(() -> "Max must be at least min, min_inclusive: " + value.minInclusive + ", max_inclusive: " + value.maxInclusive)
        : DataResult.success(value));

    public static ClampedInt of(IntProvider source, int minInclusive, int maxInclusive) {
        return new ClampedInt(source, minInclusive, maxInclusive);
    }

    public int sample(RandomSource random) {
        return Mth.clamp(source.sample(random), minInclusive, maxInclusive);
    }

    public int minInclusive() {
        return Math.max(minInclusive, source.minInclusive());
    }

    public int maxInclusive() {
        return Math.min(maxInclusive, source.maxInclusive());
    }

    public MapCodec<ClampedInt> codec() {
        return MAP_CODEC;
    }
}
