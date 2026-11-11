// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;

import com.mojang.serialization.MapCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;

public class VeryBiasedToBottomHeight extends HeightProvider {
    public static final MapCodec<VeryBiasedToBottomHeight> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(VerticalAnchor.CODEC.fieldOf("min_inclusive").forGetter(h -> h.minInclusive),
        VerticalAnchor.CODEC.fieldOf("max_inclusive").forGetter(h -> h.maxInclusive),
        com.mojang.serialization.Codec.intRange(1, Integer.MAX_VALUE).optionalFieldOf("inner", 1).forGetter(h -> h.inner)).apply(i, VeryBiasedToBottomHeight::new));
    private final VerticalAnchor minInclusive;
    private final VerticalAnchor maxInclusive;
    private final int inner;

    private VeryBiasedToBottomHeight(VerticalAnchor minInclusive, VerticalAnchor maxInclusive, int inner) {
        this.minInclusive = minInclusive;
        this.maxInclusive = maxInclusive;
        this.inner = inner;
    }

    public static VeryBiasedToBottomHeight of(VerticalAnchor minInclusive, VerticalAnchor maxInclusive, int offset) {
        return new VeryBiasedToBottomHeight(minInclusive, maxInclusive, offset);
    }

    public int sample(RandomSource random, WorldGenerationContext context) {
        int min = minInclusive.resolveY(context);
        int max = maxInclusive.resolveY(context);
        if (max - min - inner + 1 <= 0) {
            return min;
        }
        int upper = Mth.nextInt(random, min + inner, max);
        int biased = Mth.nextInt(random, min, upper - 1);
        return Mth.nextInt(random, min, biased - 1 + inner);
    }

    public HeightProviderType<?> getType() {
        return HeightProviderType.VERY_BIASED_TO_BOTTOM;
    }

    public String toString() {
        return "biased[" + minInclusive + "-" + maxInclusive + " inner: " + inner + "]";
    }
}
