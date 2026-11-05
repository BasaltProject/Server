// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import org.basaltmc.bukkit.Unsupported;
import net.minecraft.core.HolderGetter;
import net.minecraft.util.RandomSource;
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
    private long seed;
    private boolean useLegacyRandom;
    private PositionalRandomFactory randomFactory;
    private String world;

    protected RandomState() {
    }

    public static RandomState forWorld(String world, long seed, boolean useLegacyRandom) {
        RandomState state = ofSeed(seed, useLegacyRandom);
        state.world = world;
        return state;
    }
    public static RandomState ofSeed(long seed, boolean useLegacyRandom) {
        RandomState state = new RandomState();
        state.seed = seed;
        state.useLegacyRandom = useLegacyRandom;
        RandomSource base = useLegacyRandom ? new LegacyRandomSource(seed) : new XoroshiroRandomSource(seed);
        state.randomFactory = base.forkPositional();
        return state;
    }
    public Climate.Sampler createClimateSampler(SamplerContext context) {
        if (world == null) {
            throw Unsupported.hit("net.minecraft.world.level.levelgen.RandomState.createClimateSampler");
        }
        return Climate.Sampler.forWorld(world);
    }
    public static RandomState create(HolderGetter<NormalNoise> noises, long seed, NoiseGeneratorSettings settings) {
        return ofSeed(seed, settings.useLegacyRandomSource());
    }
    public static RandomState create(HolderGetter<NormalNoise> noises, long seed, boolean useLegacyRandom, BlockState defaultBlock, int seaLevel, NoiseRouter noiseRouter) {
        return ofSeed(seed, useLegacyRandom);
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
        return randomFactory.fromHashOf(name.toString()).forkPositional();
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
        return;
    }
    public long seed() {
        return seed;
    }
}
