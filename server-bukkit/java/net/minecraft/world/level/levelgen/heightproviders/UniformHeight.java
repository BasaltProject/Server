// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;

import com.mojang.serialization.MapCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;

public class UniformHeight extends HeightProvider {
    public static final MapCodec<UniformHeight> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(VerticalAnchor.CODEC.fieldOf("min_inclusive").forGetter(h -> h.minInclusive),
        VerticalAnchor.CODEC.fieldOf("max_inclusive").forGetter(h -> h.maxInclusive)).apply(i, UniformHeight::new));
    private final VerticalAnchor minInclusive;
    private final VerticalAnchor maxInclusive;

    private UniformHeight(VerticalAnchor minInclusive, VerticalAnchor maxInclusive) {
        this.minInclusive = minInclusive;
        this.maxInclusive = maxInclusive;
    }

    public static UniformHeight of(VerticalAnchor minInclusive, VerticalAnchor maxInclusive) {
        return new UniformHeight(minInclusive, maxInclusive);
    }

    public int sample(RandomSource random, WorldGenerationContext context) {
        int min = minInclusive.resolveY(context);
        int max = maxInclusive.resolveY(context);
        return min > max ? min : Mth.randomBetweenInclusive(random, min, max);
    }

    public HeightProviderType<?> getType() {
        return HeightProviderType.UNIFORM;
    }

    public String toString() {
        return "[" + minInclusive + "-" + maxInclusive + "]";
    }
}
