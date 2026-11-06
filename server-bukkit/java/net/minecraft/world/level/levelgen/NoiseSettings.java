// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.function.Function;
import net.minecraft.world.level.LevelHeightAccessor;

public record NoiseSettings(int minY, int height) {
    private static final int kLowestY = -2032;
    private static final int kHighestY = 2031;
    private static final int kTallest = 4064;
    public static final Codec<NoiseSettings> CODEC = RecordCodecBuilder.<NoiseSettings>create(i -> i.group(
        Codec.intRange(kLowestY, kHighestY).fieldOf("min_y").forGetter(NoiseSettings::minY),
        Codec.intRange(0, kTallest).fieldOf("height").forGetter(NoiseSettings::height)).apply(i, NoiseSettings::new))
        .comapFlatMap(NoiseSettings::guard, Function.identity());
    static final NoiseSettings OVERWORLD_NOISE_SETTINGS = create(-64, 384);
    static final NoiseSettings NETHER_NOISE_SETTINGS = create(0, 128);
    static final NoiseSettings END_NOISE_SETTINGS = create(0, 128);
    static final NoiseSettings CAVES_NOISE_SETTINGS = create(-64, 192);
    static final NoiseSettings FLOATING_ISLANDS_NOISE_SETTINGS = create(0, 256);

    private static DataResult<NoiseSettings> guard(NoiseSettings settings) {
        if (settings.minY + settings.height > kHighestY + 1) {
            return DataResult.error(() -> "min_y + height cannot be higher than: " + (kHighestY + 1));
        }
        if (settings.height % 16 != 0) {
            return DataResult.error(() -> "height has to be a multiple of 16");
        }
        return settings.minY % 16 != 0 ? DataResult.error(() -> "min_y has to be a multiple of 16") : DataResult.success(settings);
    }

    public static NoiseSettings create(int minY, int height) {
        NoiseSettings settings = new NoiseSettings(minY, height);
        guard(settings).error().ifPresent(error -> {
            throw new IllegalStateException(error.message());
        });
        return settings;
    }

    public NoiseSettings clampToHeightAccessor(LevelHeightAccessor heightAccessor) {
        int low = Math.max(minY, heightAccessor.getMinY());
        return new NoiseSettings(low, Math.min(minY + height, heightAccessor.getMaxY() + 1) - low);
    }
}
