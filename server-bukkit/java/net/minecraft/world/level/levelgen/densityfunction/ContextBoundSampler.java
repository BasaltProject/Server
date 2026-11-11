// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
import net.minecraft.util.context.ContextKey;
public record ContextBoundSampler(ContextKey<? extends DensitySampler> key, DensitySampler fallbackSampler) implements DensitySampler {
    public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        context.getFieldOrDefault(key, fallbackSampler).sampleVolume(context, outputBuffer, volume);
    }
    public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return context.getFieldOrDefault(key, fallbackSampler).sampleValue(context, blockX, blockY, blockZ);
    }
}
