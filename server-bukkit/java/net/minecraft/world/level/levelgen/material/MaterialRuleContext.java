// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material;

import java.util.IdentityHashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.function.DoubleSupplier;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.Mth;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.material.condition.ConditionEvaluator;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
import net.minecraft.world.level.levelgen.synth.Noise;

public final class MaterialRuleContext {
    private static final int kBelowPreliminarySurface = 8;
    private final MaterialSystem system;
    private final RandomState randomState;
    private final DensityVolume expectedVolume;
    private final DensitySamplerSet densitySamplers;
    private final Function<BlockPos, Holder<Biome>> biomeGetter;
    private final WorldGenerationContext context;
    private final Set<Holder<Biome>> possibleBiomes;
    private final DensityVolume preliminarySurfaceVolume;
    private DensityBuffer preliminarySurfaceBuffer;
    private final Map<ResourceKey<NormalNoise>, DoubleSupplier> noise2d = new IdentityHashMap<>();
    private final Map<ResourceKey<NormalNoise>, DoubleSupplier> noise3d = new IdentityHashMap<>();
    private long xzStamp = Long.MIN_VALUE + 1;
    private long yStamp = Long.MIN_VALUE + 1;
    private long secondaryStamp = xzStamp - 1;
    private long minLevelStamp = xzStamp - 1;
    private int blockX;
    private int blockY;
    private int blockZ;
    private int gradientX;
    private int gradientZ;
    private int surfaceDepth;
    private double surfaceSecondary;
    private int minSurfaceLevel;
    private final BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
    private Holder<Biome> biome;
    private int waterHeight;
    private int stoneBelow;
    private int stoneAbove;

    MaterialRuleContext(MaterialSystem system, RandomState randomState, DensityVolume expectedVolume,
        DensitySamplerSet densitySamplers, Function<BlockPos, Holder<Biome>> biomeGetter, WorldGenerationContext context,
        Set<Holder<Biome>> possibleBiomes) {
        this.system = system;
        this.randomState = randomState;
        this.expectedVolume = expectedVolume;
        this.densitySamplers = densitySamplers;
        this.biomeGetter = biomeGetter;
        this.context = context;
        this.possibleBiomes = possibleBiomes;
        this.preliminarySurfaceVolume = new DensityVolume(expectedVolume.sizeX(), 1, expectedVolume.sizeZ(),
            expectedVolume.minBlockX(), 0, expectedVolume.minBlockZ());
    }

    void updateXZ(int blockX, int blockZ, int surfaceGradientX, int surfaceGradientZ) {
        xzStamp++;
        yStamp++;
        this.blockX = blockX;
        this.blockZ = blockZ;
        this.gradientX = surfaceGradientX;
        this.gradientZ = surfaceGradientZ;
        this.surfaceDepth = system.getSurfaceDepth(blockX, blockZ);
    }

    void updateY(int stoneDepthAbove, int stoneDepthBelow, int waterHeight, int blockY) {
        yStamp++;
        this.biome = null;
        this.blockY = blockY;
        this.waterHeight = waterHeight;
        this.stoneBelow = stoneDepthBelow;
        this.stoneAbove = stoneDepthAbove;
    }

    public double getSurfaceSecondary() {
        if (secondaryStamp != xzStamp) {
            secondaryStamp = xzStamp;
            surfaceSecondary = system.getSurfaceSecondary(blockX, blockZ);
        }
        return surfaceSecondary;
    }

    public Holder<Biome> getBiome() {
        if (biome == null) {
            biome = biomeGetter.apply(pos.set(blockX, blockY, blockZ));
        }
        return biome;
    }

    public int getSeaLevel() {
        return system.getSeaLevel();
    }

    public int getMinSurfaceLevel() {
        if (minLevelStamp != xzStamp) {
            minLevelStamp = xzStamp;
            int index = preliminarySurfaceVolume.indexOfBlock(blockX, 0, blockZ);
            float level;
            if (index != -1) {
                if (preliminarySurfaceBuffer == null) {
                    preliminarySurfaceBuffer = DensityBuffer.createUnpooled(preliminarySurfaceVolume.size());
                    densitySamplers.get(system.preliminarySurfaceFunction())
                        .sampleVolume(preliminarySurfaceBuffer, preliminarySurfaceVolume);
                }
                level = preliminarySurfaceBuffer.get(index);
            } else {
                level = densitySamplers.sampleValue(system.preliminarySurfaceFunction(), blockX, 0, blockZ);
            }
            minSurfaceLevel = Mth.floor(level) + surfaceDepth - kBelowPreliminarySurface;
        }
        return minSurfaceLevel;
    }

    public DoubleSupplier getNoiseSampler(ResourceKey<NormalNoise> noiseId, boolean is3d) {
        Map<ResourceKey<NormalNoise>, DoubleSupplier> cache = is3d ? noise3d : noise2d;
        DoubleSupplier known = cache.get(noiseId);
        if (known == null) {
            known = is3d ? columnNoise(randomState.getOrCreateNoise(noiseId), true)
                : columnNoise(randomState.getOrCreateNoise(noiseId), false);
            cache.put(noiseId, known);
        }
        return known;
    }

    private DoubleSupplier columnNoise(Noise noise, boolean volumetric) {
        long[] stamp = {(volumetric ? yStamp : xzStamp) - 1};
        double[] last = new double[1];
        return () -> {
            long now = volumetric ? yStamp : xzStamp;
            if (stamp[0] != now) {
                last[0] = volumetric ? noise.get(blockX, blockY, blockZ) : noise.get(blockX, 0.0, blockZ);
                stamp[0] = now;
            }
            return last[0];
        };
    }

    public PositionalRandomFactory getOrCreateRandomFactory(Identifier name) {
        return randomState.getOrCreateRandomFactory(name);
    }

    public MaterialRules.DensityGetter getDensitiesInChunk(DensityFunction function, boolean prefill) {
        DensitySampler.Bound sampler = densitySamplers.get(function);
        if (!prefill) {
            return () -> sampler.sampleValue(blockX, blockY, blockZ);
        }
        DensityBuffer filled = DensityBuffer.createUnpooled(expectedVolume.size());
        sampler.sampleVolume(filled, expectedVolume);
        return () -> {
            int index = expectedVolume.indexOfBlock(blockX, blockY, blockZ);
            return index == -1 ? sampler.sampleValue(blockX, blockY, blockZ) : filled.get(index);
        };
    }

    public Set<Holder<Biome>> possibleBiomes() {
        return possibleBiomes;
    }

    public int stoneDepthAbove() {
        return stoneAbove;
    }

    public int stoneDepthBelow() {
        return stoneBelow;
    }

    public int surfaceDepth() {
        return surfaceDepth;
    }

    public int waterHeight() {
        return waterHeight;
    }

    public int blockX() {
        return blockX;
    }

    public int blockY() {
        return blockY;
    }

    public int blockZ() {
        return blockZ;
    }

    public BlockPos blockPos() {
        return pos.set(blockX, blockY, blockZ);
    }

    public int surfaceGradientX() {
        return gradientX;
    }

    public int surfaceGradientZ() {
        return gradientZ;
    }

    public int resolveAnchorY(VerticalAnchor anchor) {
        return anchor.resolveY(context);
    }

    public BlockState getBand(int x, int y, int z) {
        return system.getBand(x, y, z);
    }

    public static abstract class LazyXZCondition implements ConditionEvaluator {
        protected final MaterialRuleContext context;
        private long stamp;
        private boolean result;

        protected LazyXZCondition(MaterialRuleContext context) {
            this.context = context;
            this.stamp = context.xzStamp - 1;
        }

        public boolean test() {
            if (stamp != context.xzStamp) {
                stamp = context.xzStamp;
                result = compute();
            }
            return result;
        }

        protected abstract boolean compute();
    }

    public static abstract class LazyYCondition implements ConditionEvaluator {
        protected final MaterialRuleContext context;
        private long stamp;
        private boolean result;

        protected LazyYCondition(MaterialRuleContext context) {
            this.context = context;
            this.stamp = context.yStamp - 1;
        }

        public boolean test() {
            if (stamp != context.yStamp) {
                stamp = context.yStamp;
                result = compute();
            }
            return result;
        }

        protected abstract boolean compute();
    }

    public ConditionEvaluator lazyXz(BooleanSupplier body) {
        return new LazyXZCondition(this) {
            protected boolean compute() {
                return body.getAsBoolean();
            }
        };
    }

    public ConditionEvaluator lazyY(BooleanSupplier body) {
        return new LazyYCondition(this) {
            protected boolean compute() {
                return body.getAsBoolean();
            }
        };
    }
}
