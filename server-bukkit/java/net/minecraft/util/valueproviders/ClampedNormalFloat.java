// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record ClampedNormalFloat(float mean, float deviation, float min, float max) implements FloatProvider {
    public static final MapCodec<ClampedNormalFloat> MAP_CODEC = RecordCodecBuilder.<ClampedNormalFloat>mapCodec(i -> i.group(
        Codec.FLOAT.fieldOf("mean").forGetter(ClampedNormalFloat::mean), Codec.FLOAT.fieldOf("deviation").forGetter(ClampedNormalFloat::deviation),
        Codec.FLOAT.fieldOf("min").forGetter(ClampedNormalFloat::min), Codec.FLOAT.fieldOf("max").forGetter(ClampedNormalFloat::max))
        .apply(i, ClampedNormalFloat::new)).validate(value -> value.max < value.min
        ? DataResult.error(() -> "Max must be larger than min: [" + value.min + ", " + value.max + "]") : DataResult.success(value));

    public static ClampedNormalFloat of(float mean, float deviation, float min, float max) {
        return new ClampedNormalFloat(mean, deviation, min, max);
    }

    public float sample(RandomSource random) {
        return sample(random, mean, deviation, min, max);
    }

    public static float sample(RandomSource random, float mean, float deviation, float min, float max) {
        return Mth.clamp(Mth.normal(random, mean, deviation), min, max);
    }

    public MapCodec<ClampedNormalFloat> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return "normal(" + mean + ", " + deviation + ") in [" + min + "-" + max + "]";
    }
}
