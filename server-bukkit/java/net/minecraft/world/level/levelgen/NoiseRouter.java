// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import com.mojang.serialization.Codec;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
public record NoiseRouter(DensityFunction temperature, DensityFunction vegetation, DensityFunction continents, DensityFunction erosion, DensityFunction depth, DensityFunction ridges, DensityFunction chunkSurfaceLevel, DensityFunction finalDensity) {
    public static final Codec<NoiseRouter> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.NoiseRouter.class, "CODEC");
    public Climate.Sampler createClimateSampler(DensitySamplerSet densitySamplers) {
        return new Climate.Sampler(densitySamplers.get(temperature), densitySamplers.get(vegetation),
            densitySamplers.get(continents), densitySamplers.get(erosion), densitySamplers.get(depth),
            densitySamplers.get(ridges));
    }
}
