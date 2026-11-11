// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
public record CachingDensitySampler(int id, DensitySampler input) implements DensitySampler {
    public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        context.sampleVolumeCached(id, input, outputBuffer, volume);
    }
    public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return context.sampleValueCached(id, input, blockX, blockY, blockZ);
    }
}
