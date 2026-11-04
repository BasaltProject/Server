// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record ConstantFloat(float value) implements FloatProvider {
    public static final ConstantFloat ZERO = new ConstantFloat(0.0F);
    public static final MapCodec<ConstantFloat> MAP_CODEC = RecordCodecBuilder.<ConstantFloat>mapCodec(i -> i.group(
        Codec.FLOAT.fieldOf("value").forGetter(ConstantFloat::value)).apply(i, ConstantFloat::of));

    public static ConstantFloat of(float value) {
        return value == 0.0F ? ZERO : new ConstantFloat(value);
    }

    public float sample(RandomSource random) {
        return value;
    }

    public float min() {
        return value;
    }

    public float max() {
        return value;
    }

    public MapCodec<ConstantFloat> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return Float.toString(value);
    }
}
