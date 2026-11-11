// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;

import com.mojang.serialization.MapCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;

public class TrapezoidHeight extends HeightProvider {
    public static final MapCodec<TrapezoidHeight> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(VerticalAnchor.CODEC.fieldOf("min_inclusive").forGetter(h -> h.minInclusive),
        VerticalAnchor.CODEC.fieldOf("max_inclusive").forGetter(h -> h.maxInclusive),
        com.mojang.serialization.Codec.INT.optionalFieldOf("plateau", 0).forGetter(h -> h.plateau)).apply(i, TrapezoidHeight::new));
    private final VerticalAnchor minInclusive;
    private final VerticalAnchor maxInclusive;
    private final int plateau;

    private TrapezoidHeight(VerticalAnchor minInclusive, VerticalAnchor maxInclusive, int plateau) {
        this.minInclusive = minInclusive;
        this.maxInclusive = maxInclusive;
        this.plateau = plateau;
    }

    public static TrapezoidHeight of(VerticalAnchor minInclusive, VerticalAnchor maxInclusive, int plateau) {
        return new TrapezoidHeight(minInclusive, maxInclusive, plateau);
    }

    public static TrapezoidHeight of(VerticalAnchor minInclusive, VerticalAnchor maxInclusive) {
        return of(minInclusive, maxInclusive, 0);
    }

    public int sample(RandomSource random, WorldGenerationContext context) {
        int min = minInclusive.resolveY(context);
        int max = maxInclusive.resolveY(context);
        if (min > max) {
            return min;
        }
        int span = max - min;
        if (plateau >= span) {
            return Mth.randomBetweenInclusive(random, min, max);
        }
        int shoulder = (span - plateau) / 2;
        int rise = span - shoulder;
        return min + Mth.randomBetweenInclusive(random, 0, rise) + Mth.randomBetweenInclusive(random, 0, shoulder);
    }

    public HeightProviderType<?> getType() {
        return HeightProviderType.TRAPEZOID;
    }

    public String toString() {
        if (plateau == 0) {
            return "triangle (" + minInclusive + "-" + maxInclusive + ")";
        }
        return "trapezoid(" + plateau + ") in [" + minInclusive + "-" + maxInclusive + "]";
    }
}
