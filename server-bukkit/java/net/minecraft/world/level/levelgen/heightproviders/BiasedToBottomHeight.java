// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;

import com.mojang.serialization.MapCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;

public class BiasedToBottomHeight extends HeightProvider {
    public static final MapCodec<BiasedToBottomHeight> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(VerticalAnchor.CODEC.fieldOf("min_inclusive").forGetter(h -> h.minInclusive),
        VerticalAnchor.CODEC.fieldOf("max_inclusive").forGetter(h -> h.maxInclusive),
        com.mojang.serialization.Codec.intRange(1, Integer.MAX_VALUE).optionalFieldOf("inner", 1).forGetter(h -> h.inner)).apply(i, BiasedToBottomHeight::new));
    private final VerticalAnchor minInclusive;
    private final VerticalAnchor maxInclusive;
    private final int inner;

    private BiasedToBottomHeight(VerticalAnchor minInclusive, VerticalAnchor maxInclusive, int inner) {
        this.minInclusive = minInclusive;
        this.maxInclusive = maxInclusive;
        this.inner = inner;
    }

    public static BiasedToBottomHeight of(VerticalAnchor minInclusive, VerticalAnchor maxInclusive, int offset) {
        return new BiasedToBottomHeight(minInclusive, maxInclusive, offset);
    }

    public int sample(RandomSource random, WorldGenerationContext context) {
        int min = minInclusive.resolveY(context);
        int max = maxInclusive.resolveY(context);
        int room = max - min - inner + 1;
        if (room <= 0) {
            return min;
        }
        int limit = random.nextInt(room);
        return random.nextInt(limit + inner) + min;
    }

    public HeightProviderType<?> getType() {
        return HeightProviderType.BIASED_TO_BOTTOM;
    }

    public String toString() {
        return "biased[" + minInclusive + "-" + maxInclusive + " inner: " + inner + "]";
    }
}
