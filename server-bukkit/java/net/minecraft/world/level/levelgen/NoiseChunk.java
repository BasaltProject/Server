// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import net.minecraft.resources.Identifier;
import net.minecraft.util.context.ContextMap;
import net.minecraft.world.level.levelgen.densityfunction.DensityBufferPool;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
public class NoiseChunk implements AutoCloseable {
    private final RandomState randomState;
    private final DensityVolume volume;
    private final Aquifer aquifer;
    private final DensityBufferPool bufferPool;
    private final DensitySamplerSet cachingSamplers;
    public NoiseChunk(RandomState randomState, Beardifier beardifier, NoiseGeneratorSettings settings, Aquifer.FluidPicker globalFluidPicker, Blender blender, DensityVolume volume) {
        this.randomState = randomState;
        this.volume = volume;
        this.bufferPool = randomState.acquireDensityBufferPool();
        ContextMap.Builder fields = ContextMap.builder().set(Beardifier.CONTEXT_KEY, beardifier);
        if (!blender.isEmpty()) {
            Blender.OutputBuffer blended = blender.blendOffsetAndFactor(volume);
            fields.set(Blender.CONTEXT_KEY, blender).set(Blender.ALPHA_KEY, blended.createAlphaSampler())
                .set(Blender.OFFSET_KEY, blended.createOffsetSampler());
        }
        this.cachingSamplers = randomState.samplersWithContext(
            SamplerContext.builder().setUserFields(fields.build()).useBufferArena(bufferPool).enableCaches().build());
        this.aquifer = settings.aquifers().isPresent()
            ? settings.aquifers().get().create(cachingSamplers,
                randomState.getOrCreateRandomFactory(Identifier.withDefaultNamespace("aquifer")), volume, globalFluidPicker)
            : Aquifer.createDisabled(globalFluidPicker);
    }
    public DensityVolume volume() {
        return volume;
    }
    public Aquifer aquifer() {
        return aquifer;
    }
    public DensitySamplerSet cachingSamplers() {
        return cachingSamplers;
    }
    public void close() {
        randomState.releaseDensityBufferPool(bufferPool);

    }
}
