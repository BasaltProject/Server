// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import org.basaltmc.bukkit.Unsupported;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.densityfunction.DensityBufferPool;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
import net.minecraft.world.level.levelgen.material.MaterialSystem;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public class RandomState {
    protected RandomState() {
    }
    public Climate.Sampler createClimateSampler(SamplerContext context) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.RandomState.createClimateSampler");
    }
    public static RandomState create(HolderGetter<NormalNoise> noises, long seed, NoiseGeneratorSettings settings) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#create");
    }
    public static RandomState create(HolderGetter<NormalNoise> noises, long seed, boolean useLegacyRandom, BlockState defaultBlock, int seaLevel, NoiseRouter noiseRouter) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#create");
    }
    private RandomState(HolderGetter<NormalNoise> noises, long seed, boolean useLegacyRandom, BlockState defaultBlock, int seaLevel, NoiseRouter router) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#<init>");
    }
    public DensitySamplerSet samplersWithContext(SamplerContext context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#samplersWithContext");
    }
    public Noise getOrCreateNoise(ResourceKey<NormalNoise> noise) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#getOrCreateNoise");
    }
    public PositionalRandomFactory getOrCreateRandomFactory(Identifier name) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#getOrCreateRandomFactory");
    }
    public MaterialSystem surfaceSystem() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#surfaceSystem");
    }
    public DensitySampler getSampler(DensityFunction function) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#getSampler");
    }
    public float sampleBlockValueUncached(DensityFunction function, int x, int y, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#sampleBlockValueUncached");
    }
    public DensityBufferPool acquireDensityBufferPool() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#acquireDensityBufferPool");
    }
    public void releaseDensityBufferPool(DensityBufferPool pool) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#releaseDensityBufferPool");
    }
    public void garbageCollect() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#garbageCollect");
    }
    public long seed() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.RandomState#seed");
    }
}
