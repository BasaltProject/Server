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
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.facade.NativeDensityFunction;
import org.basaltmc.bukkit.facade.NativeNoise;
public class RandomState {
    private static final String kDefaultSettings = "minecraft:overworld";
    private long seed;
    private boolean useLegacyRandom;
    private PositionalRandomFactory randomFactory;
    private String world;
    private String settings = kDefaultSettings;
    private final Map<ResourceKey<NormalNoise>, Noise> noiseInstances = new ConcurrentHashMap<>();
    private final Map<Identifier, PositionalRandomFactory> positionalRandoms = new ConcurrentHashMap<>();
    private final Map<DensityFunction, DensitySampler> samplers = new ConcurrentHashMap<>();
    private final Deque<DensityBufferPool> bufferPools = new ArrayDeque<>();
    private MaterialSystem materialSystem;

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
        return create(noises, seed, settings.useLegacyRandomSource(), settings.defaultBlock(), settings.seaLevel(), settings.noiseRouter());
    }
    public static RandomState create(HolderGetter<NormalNoise> noises, long seed, boolean useLegacyRandom, BlockState defaultBlock, int seaLevel, NoiseRouter noiseRouter) {
        return new RandomState(noises, seed, useLegacyRandom, defaultBlock, seaLevel, noiseRouter);
    }
    private RandomState(HolderGetter<NormalNoise> noises, long seed, boolean useLegacyRandom, BlockState defaultBlock, int seaLevel, NoiseRouter router) {
        this.seed = seed;
        this.useLegacyRandom = useLegacyRandom;
        RandomSource base = useLegacyRandom ? new LegacyRandomSource(seed) : new XoroshiroRandomSource(seed);
        this.randomFactory = base.forkPositional();
        if (router != null && router.finalDensity() instanceof NativeDensityFunction nativeFunction) {
            this.settings = nativeFunction.settings();
        }
        this.materialSystem = new MaterialSystem(this, defaultBlock, seaLevel, router == null ? null : router.chunkSurfaceLevel(), randomFactory.fromHashOf("surface").forkPositional());
    }
    public DensitySamplerSet samplersWithContext(SamplerContext context) {
        return function -> getSampler(function).bind(context);
    }
    public Noise getOrCreateNoise(ResourceKey<NormalNoise> noise) {
        return noiseInstances.computeIfAbsent(noise, key -> NativeNoise.create(key.identifier().toString(), randomFactory.fromHashOf(key.identifier()), false));
    }
    public PositionalRandomFactory getOrCreateRandomFactory(Identifier name) {
        return positionalRandoms.computeIfAbsent(name, key -> randomFactory.fromHashOf(key).forkPositional());
    }
    public MaterialSystem surfaceSystem() {
        return materialSystem;
    }
    public DensitySampler getSampler(DensityFunction function) {
        if (function instanceof NativeDensityFunction nativeFunction) {
            return samplers.computeIfAbsent(function, key -> new NativeDensityFunction(nativeFunction.settings(), seed, nativeFunction.name()).compileSampler(null));
        }
        return samplers.computeIfAbsent(function, key -> key.compileSampler(null));
    }
    public float sampleBlockValueUncached(DensityFunction function, int x, int y, int z) {
        return getSampler(function).sampleValue(SamplerContext.EMPTY_UNCACHED, x, y, z);
    }
    public DensityBufferPool acquireDensityBufferPool() {
        synchronized (bufferPools) {
            return bufferPools.isEmpty() ? new DensityBufferPool(20) : bufferPools.removeLast();
        }
    }
    public void releaseDensityBufferPool(DensityBufferPool pool) {
        synchronized (bufferPools) {
            bufferPools.addLast(pool);
        }
    }
    public void garbageCollect() {
        synchronized (bufferPools) {
            bufferPools.forEach(DensityBufferPool::garbageCollect);
        }
    }
    public long seed() {
        return seed;
    }
}
