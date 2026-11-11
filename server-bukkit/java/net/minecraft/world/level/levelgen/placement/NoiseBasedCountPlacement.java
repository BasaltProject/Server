// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record NoiseBasedCountPlacement(int noiseToCountRatio, double noiseFactor, double noiseOffset) implements RepeatingPlacement {
    public static final MapCodec<NoiseBasedCountPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.NoiseBasedCountPlacement.class, "CODEC");
    public static NoiseBasedCountPlacement of(int noiseToCountRatio, double noiseFactor, double noiseOffset) {
        return new NoiseBasedCountPlacement(noiseToCountRatio, noiseFactor, noiseOffset);
    }
    public int count(RandomSource random, BlockPos origin) {
        double noise = net.minecraft.world.level.biome.Biome.BIOME_INFO_NOISE.get(origin.getX() / noiseFactor, origin.getZ() / noiseFactor);
        return (int) Math.ceil((noise + noiseOffset) * noiseToCountRatio);
    }
    public MapCodec<NoiseBasedCountPlacement> codec() {
        return CODEC;
    }
}
