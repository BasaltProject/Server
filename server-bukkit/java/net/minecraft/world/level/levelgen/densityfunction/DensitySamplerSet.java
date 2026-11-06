// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

@FunctionalInterface
public interface DensitySamplerSet {
    DensitySampler.Bound get(DensityFunction function);

    default float sampleValue(DensityFunction function, int blockX, int blockY, int blockZ) {
        return get(function).sampleValue(blockX, blockY, blockZ);
    }
}
