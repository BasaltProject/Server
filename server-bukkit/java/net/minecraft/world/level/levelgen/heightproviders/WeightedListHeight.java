// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;

import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.levelgen.WorldGenerationContext;

public class WeightedListHeight extends HeightProvider {
    public static final MapCodec<WeightedListHeight> CODEC = net.minecraft.util.random.WeightedList.codec(HeightProvider.CODEC).fieldOf("distribution").xmap(WeightedListHeight::new, h -> h.distribution);
    private final WeightedList<HeightProvider> distribution;

    public WeightedListHeight(WeightedList<HeightProvider> distribution) {
        this.distribution = distribution;
    }

    public int sample(RandomSource random, WorldGenerationContext heightAccessor) {
        return distribution.getRandomOrThrow(random).sample(random, heightAccessor);
    }

    public HeightProviderType<?> getType() {
        return HeightProviderType.WEIGHTED_LIST;
    }
}
