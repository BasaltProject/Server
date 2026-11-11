// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record NoiseThresholdCountPlacement(double noiseLevel, int belowNoise, int aboveNoise) implements RepeatingPlacement {
    public static final MapCodec<NoiseThresholdCountPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.NoiseThresholdCountPlacement.class, "CODEC");
    public static NoiseThresholdCountPlacement of(double noiseLevel, int belowNoise, int aboveNoise) {
        return new NoiseThresholdCountPlacement(noiseLevel, belowNoise, aboveNoise);
    }
    public int count(RandomSource random, BlockPos origin) {
        double noise = net.minecraft.world.level.biome.Biome.BIOME_INFO_NOISE.get(origin.getX() / 200.0, origin.getZ() / 200.0);
        return noise < noiseLevel ? belowNoise : aboveNoise;
    }
    public MapCodec<NoiseThresholdCountPlacement> codec() {
        return CODEC;
    }
}
