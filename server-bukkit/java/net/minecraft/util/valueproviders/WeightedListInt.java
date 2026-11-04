// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

import net.minecraft.util.random.Weighted;
import net.minecraft.util.random.WeightedList;

public class WeightedListInt implements IntProvider {
    public static final MapCodec<WeightedListInt> MAP_CODEC = RecordCodecBuilder.<WeightedListInt>mapCodec(i -> i.group(
        WeightedList.codec(IntProviders.CODEC).validate(list -> list.unwrap().isEmpty()
            ? DataResult.<WeightedList<IntProvider>>error(() -> "List must have contents") : DataResult.success(list))
            .fieldOf("distribution").forGetter(provider -> provider.distribution)).apply(i, WeightedListInt::new));
    private final WeightedList<IntProvider> distribution;
    private final int minValue;
    private final int maxValue;

    public WeightedListInt(WeightedList<IntProvider> distribution) {
        this.distribution = distribution;
        int min = Integer.MAX_VALUE;
        int max = Integer.MIN_VALUE;
        for (Weighted<IntProvider> entry : distribution.unwrap()) {
            min = Math.min(min, entry.value().minInclusive());
            max = Math.max(max, entry.value().maxInclusive());
        }
        this.minValue = min;
        this.maxValue = max;
    }

    public int sample(RandomSource random) {
        return distribution.getRandomOrThrow(random).sample(random);
    }

    public int minInclusive() {
        return minValue;
    }

    public int maxInclusive() {
        return maxValue;
    }

    public MapCodec<WeightedListInt> codec() {
        return MAP_CODEC;
    }
}
