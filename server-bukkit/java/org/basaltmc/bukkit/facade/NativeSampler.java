package org.basaltmc.bukkit.facade;

import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import org.basaltmc.bukkit.LeftoverNatives;

/** Samples a router function of a noise settings and seed in the native density code. */
public final class NativeSampler implements DensitySampler {
    private final NativeDensityFunction function;

    NativeSampler(NativeDensityFunction function) {
        this.function = function;
    }

    public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return (float) LeftoverNatives.terrainDensity(function.settings(), function.seed(), function.name(), blockX, blockY, blockZ);
    }

    public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        DensitySampler.sampleVolumeNaive(context, outputBuffer, volume, this);
    }
}
