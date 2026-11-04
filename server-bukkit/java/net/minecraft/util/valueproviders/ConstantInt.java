// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record ConstantInt(int value) implements IntProvider {
    public static final ConstantInt ZERO = new ConstantInt(0);
    public static final MapCodec<ConstantInt> MAP_CODEC = RecordCodecBuilder.<ConstantInt>mapCodec(i -> i.group(
        Codec.INT.fieldOf("value").forGetter(ConstantInt::value)).apply(i, ConstantInt::of));

    public static ConstantInt of(int value) {
        return value == 0 ? ZERO : new ConstantInt(value);
    }

    public int sample(RandomSource random) {
        return value;
    }

    public int minInclusive() {
        return value;
    }

    public int maxInclusive() {
        return value;
    }

    public MapCodec<ConstantInt> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return Integer.toString(value);
    }
}
