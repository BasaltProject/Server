// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public record TrapezoidFloat(float min, float max, float plateau) implements FloatProvider {
    public static final MapCodec<TrapezoidFloat> MAP_CODEC = RecordCodecBuilder.<TrapezoidFloat>mapCodec(i -> i.group(
        Codec.FLOAT.fieldOf("min").forGetter(TrapezoidFloat::min), Codec.FLOAT.fieldOf("max").forGetter(TrapezoidFloat::max),
        Codec.FLOAT.fieldOf("plateau").forGetter(TrapezoidFloat::plateau)).apply(i, TrapezoidFloat::new)).validate(value -> {
            if (value.max < value.min) {
                return DataResult.error(() -> "Max must be larger than min: [" + value.min + ", " + value.max + "]");
            }
            return value.plateau > value.max - value.min
                ? DataResult.error(() -> "Plateau can at most be the full span: [" + value.min + ", " + value.max + "]") : DataResult.success(value);
        });

    public static TrapezoidFloat of(float min, float max, float plateau) {
        return new TrapezoidFloat(min, max, plateau);
    }

    public float sample(RandomSource random) {
        float range = max - min;
        float plateauStart = (range - plateau) / 2.0F;
        float plateauEnd = range - plateauStart;
        return min + random.nextFloat() * plateauEnd + random.nextFloat() * plateauStart;
    }

    public MapCodec<TrapezoidFloat> codec() {
        return MAP_CODEC;
    }

    public String toString() {
        return "trapezoid(" + plateau + ") in [" + min + "-" + max + "]";
    }
}
