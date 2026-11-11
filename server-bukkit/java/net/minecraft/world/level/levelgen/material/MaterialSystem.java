// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material;

import java.util.Arrays;
import java.util.Optional;
import java.util.Set;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.SectionPos;
import net.minecraft.resources.Identifier;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.Biomes;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.BlockColumn;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.NoiseChunk;
import net.minecraft.world.level.levelgen.Noises;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
import net.minecraft.world.level.levelgen.material.rule.RuleEvaluator;
import net.minecraft.world.level.levelgen.synth.Noise;

public class MaterialSystem {
    private static final int kWayBelowMinY = -2032 << 4;
    private static final int kBandCount = 192;
    private final BlockState defaultBlock;
    private final int seaLevel;
    private final DensityFunction preliminarySurfaceFunction;
    private final BlockState[] clayBands;
    private final Noise clayBandsOffsetNoise;
    private final Noise badlandsPillarNoise;
    private final Noise badlandsPillarRoofNoise;
    private final Noise badlandsSurfaceNoise;
    private final Noise icebergPillarNoise;
    private final Noise icebergPillarRoofNoise;
    private final Noise icebergSurfaceNoise;
    private final PositionalRandomFactory noiseRandom;
    private final Noise surfaceNoise;
    private final Noise surfaceSecondaryNoise;

    public MaterialSystem(RandomState randomState, BlockState defaultBlock, int seaLevel,
        DensityFunction preliminarySurfaceFunction, PositionalRandomFactory noiseRandom) {
        this.defaultBlock = defaultBlock;
        this.seaLevel = seaLevel;
        this.preliminarySurfaceFunction =
            preliminarySurfaceFunction;
        this.noiseRandom = noiseRandom;
        this.clayBandsOffsetNoise =
            randomState.getOrCreateNoise(Noises.CLAY_BANDS_OFFSET);
        this.clayBands =
            generateBands(noiseRandom.fromHashOf(Identifier.withDefaultNamespace("clay_bands")));
        this.surfaceNoise =
            randomState.getOrCreateNoise(Noises.SURFACE);
        this.surfaceSecondaryNoise =
            randomState.getOrCreateNoise(Noises.SURFACE_SECONDARY);
        this.badlandsPillarNoise =
            randomState.getOrCreateNoise(Noises.BADLANDS_PILLAR);
        this.badlandsPillarRoofNoise =
            randomState.getOrCreateNoise(Noises.BADLANDS_PILLAR_ROOF);
        this.badlandsSurfaceNoise =
            randomState.getOrCreateNoise(Noises.BADLANDS_SURFACE);
        this.icebergPillarNoise =
            randomState.getOrCreateNoise(Noises.ICEBERG_PILLAR);
        this.icebergPillarRoofNoise =
            randomState.getOrCreateNoise(Noises.ICEBERG_PILLAR_ROOF);
        this.icebergSurfaceNoise =
            randomState.getOrCreateNoise(Noises.ICEBERG_SURFACE);
    }

    private static BlockColumn columnOf(ChunkAccess chunk, BlockPos.MutableBlockPos cursor) {
        int low = chunk.getHeightAccessorForGeneration().getMinY();
        int high = chunk.getHeightAccessorForGeneration().getMaxY();
        return new BlockColumn() {
            public BlockState getBlock(int blockY) {
                return chunk.getBlockState(cursor.setY(blockY));
            }

            public void setBlock(int blockY, BlockState state) {
                if (blockY < low || blockY > high) {
                    return;
                }
                chunk.setBlockState(cursor.setY(blockY), state);
                if (!state.getFluidState().isEmpty()) {
                    chunk.markPosForPostProcessing(cursor);
                }
            }
        };
    }

    public void buildSurface(RandomState randomState, BiomeManager biomeManager, WorldGenerationContext generationContext,
        ChunkAccess protoChunk, NoiseChunk noiseChunk, MaterialRule ruleSource, Set<Holder<Biome>> possibleBiomes) {
        BlockPos.MutableBlockPos columnPos = new BlockPos.MutableBlockPos();
        ChunkPos chunkPos = protoChunk.getPos();
        LevelHeightAccessor heights = protoChunk.getHeightAccessorForGeneration();
        int minY = heights.getMinY();
        int maxY = heights.getMaxY();
        BlockColumn column = columnOf(protoChunk, columnPos);
        int topSection = protoChunk.getSectionYFromSectionIndex(protoChunk.getHighestFilledSectionIndex()) * 16 + 15;
        DensityVolume full = noiseChunk.volume();
        DensityVolume narrowed = new DensityVolume(full.sizeX(), Math.max(topSection - full.minBlockY() + 1, 1),
            full.sizeZ(), full.minBlockX(), full.minBlockY(), full.minBlockZ());
        MaterialRuleContext context = new MaterialRuleContext(this, randomState, narrowed, noiseChunk.cachingSamplers(),
            biomeManager::getBiome, generationContext, possibleBiomes);
        RuleEvaluator rule =
            ruleSource.compile(context);
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                int blockX = chunkPos.getMinBlockX() + x;
                int blockZ = chunkPos.getMinBlockZ() + z;
                int startHeight = protoChunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, x, z) + 1;
                columnPos.setX(blockX);
                columnPos.setZ(blockZ);
                Holder<Biome> surfaceBiome = biomeManager.getBiome(probe.set(blockX, startHeight, blockZ));
                if (surfaceBiome.is(Biomes.ERODED_BADLANDS)) {
                    erodedBadlandsExtension(column, blockX, blockZ, startHeight, protoChunk);
                }
                int top = protoChunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, x, z) + 1;
                context.updateXZ(blockX, blockZ, gradientX(protoChunk, x, z), gradientZ(protoChunk, x, z));
                paintColumn(column, context, rule, blockX, blockZ, top, minY, maxY, protoChunk.getMinY());
                if (surfaceBiome.is(Biomes.FROZEN_OCEAN) || surfaceBiome.is(Biomes.DEEP_FROZEN_OCEAN)) {
                    frozenOceanExtension(context.getMinSurfaceLevel(), surfaceBiome.value(), column, probe, blockX,
                        blockZ, startHeight);
                }
            }
        }
    }

    private void paintColumn(BlockColumn column, MaterialRuleContext context, RuleEvaluator rule, int blockX, int blockZ,
        int top, int minY, int maxY, int endY) {
        int stoneAbove = 0;
        int water = Integer.MIN_VALUE;
        int ceiling = Integer.MAX_VALUE;
        for (int y = top; y >= endY; y--) {
            BlockState old =
                column.getBlock(y);
            if (old.isAir()) {
                stoneAbove = 0;
                water = Integer.MIN_VALUE;
                continue;
            }
            if (!old.getFluidState().isEmpty()) {
                water = water == Integer.MIN_VALUE ? y + 1 : water;
                continue;
            }
            if (ceiling >= y) {
                ceiling = kWayBelowMinY;
                for (int look = y - 1; look >= endY - 1; look--) {
                    if (!isStone(column.getBlock(look))) {
                        ceiling = look + 1;
                        break;
                    }
                }
            }
            stoneAbove++;
            context.updateY(stoneAbove, y - ceiling + 1, water, y);
            if (y >= minY && y <= maxY) {
                BlockState state =
                    rule.tryApply(blockX, y, blockZ);
                if (state != null) {
                    column.setBlock(y, state);
                }
            }
        }
    }

    private static int gradientX(ChunkAccess chunk, int x, int z) {
        return chunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, Math.min(x + 1, 15), z)
            - chunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, Math.max(x - 1, 0), z);
    }

    private static int gradientZ(ChunkAccess chunk, int x, int z) {
        return chunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, x, Math.min(z + 1, 15))
            - chunk.getHeight(Heightmap.Types.WORLD_SURFACE_WG, x, Math.max(z - 1, 0));
    }

    protected int getSurfaceDepth(int blockX, int blockZ) {
        double base = surfaceNoise.get(blockX, 0.0, blockZ);
        return (int) (base * 2.75 + 3.0 + noiseRandom.at(blockX, 0, blockZ).nextDouble() * 0.25);
    }

    protected double getSurfaceSecondary(int blockX, int blockZ) {
        return surfaceSecondaryNoise.get(blockX, 0.0, blockZ);
    }

    private boolean isStone(BlockState state) {
        return
            !state.isAir() && state.getFluidState().isEmpty();
    }

    public int getSeaLevel() {
        return seaLevel;
    }

    @Deprecated
    public Optional<BlockState> topMaterial(MaterialRule ruleSource, RandomState randomState,
        WorldGenerationContext worldGenerationContext, Function<BlockPos, Holder<Biome>> biomeGetter, ChunkAccess chunk,
        DensitySamplerSet densitySamplers, BlockPos pos, boolean underFluid) {
        DensityVolume single = new DensityVolume(1, 1, 1, pos.getX(), pos.getY(), pos.getZ());
        MaterialRuleContext context = new MaterialRuleContext(this, randomState, single, densitySamplers, biomeGetter,
            worldGenerationContext, null);
        RuleEvaluator rule =
            ruleSource.compile(context);
        int localX = SectionPos.sectionRelative(pos.getX());
        int localZ = SectionPos.sectionRelative(pos.getZ());
        context.updateXZ(pos.getX(), pos.getZ(), gradientX(chunk, localX, localZ), gradientZ(chunk, localX, localZ));
        context.updateY(1, 1, underFluid ? pos.getY() + 1 : Integer.MIN_VALUE, pos.getY());
        return Optional.ofNullable(rule.tryApply(pos.getX(), pos.getY(), pos.getZ()));
    }

    private void erodedBadlandsExtension(BlockColumn column, int blockX, int blockZ, int height, LevelHeightAccessor heights) {
        double pillar = Math.min(Math.abs(badlandsSurfaceNoise.get(blockX, 0.0, blockZ) * 8.25),
            badlandsPillarNoise.get(blockX * 0.2, 0.0, blockZ * 0.2) * 15.0F);
        if (pillar <= 0.0) {
            return;
        }
        double floor = Math.abs(badlandsPillarRoofNoise.get(blockX * 0.75, 0.0, blockZ * 0.75) * 1.5);
        int startY = Mth.floor(64.0 + Math.min(pillar * pillar * 2.5, Math.ceil(floor * 50.0) + 24.0));
        if (height > startY) {
            return;
        }
        for (int y = startY; y >= heights.getMinY(); y--) {
            BlockState old =
                column.getBlock(y);
            if (old.is(defaultBlock.getBlock())) {
                break;
            }
            if (old.is(Blocks.WATER)) {
                return;
            }
        }
        for (int y = startY; y >= heights.getMinY() && column.getBlock(y).isAir(); y--) {
            column.setBlock(y, defaultBlock);
        }
    }

    private void frozenOceanExtension(int minSurfaceLevel, Biome surfaceBiome, BlockColumn column,
        BlockPos.MutableBlockPos blockPos, int blockX, int blockZ, int height) {
        double iceberg = Math.min(Math.abs(icebergSurfaceNoise.get(blockX, 0.0, blockZ) * 8.25),
            icebergPillarNoise.get(blockX * 1.28, 0.0, blockZ * 1.28) * 15.0F);
        if (iceberg <= 1.8) {
            return;
        }
        double roof = Math.abs(icebergPillarRoofNoise.get(blockX * 1.17, 0.0, blockZ * 1.17) * 1.5);
        double top = Math.min(iceberg * iceberg * 1.2, Math.ceil(roof * 40.0) + 14.0);
        if (surfaceBiome.shouldMeltFrozenOceanIcebergSlightly(blockPos.set(blockX, seaLevel, blockZ), seaLevel)) {
            top -= 2.0;
        }
        if (top <= 2.0) {
            return;
        }
        double bottom = seaLevel - top - 7.0;
        top += seaLevel;
        RandomSource random = noiseRandom.at(blockX, 0, blockZ);
        int maxSnow = 2 + random.nextInt(4);
        int snowFloor = seaLevel + 18 + random.nextInt(10);
        int snow = 0;
        for (int y = Math.max(height, (int) top + 1); y >= minSurfaceLevel; y--) {
            BlockState here = column.getBlock(y);
            boolean airFill = here.isAir() && y < (int) top && random.nextDouble() > 0.01;
            boolean waterFill = here.is(Blocks.WATER) && y > (int) bottom && y < seaLevel && random.nextDouble() > 0.15;
            if (airFill || waterFill) {
                if (snow <= maxSnow && y > snowFloor) {
                    column.setBlock(y, Blocks.SNOW_BLOCK.defaultBlockState());
                    snow++;
                } else {
                    column.setBlock(y, Blocks.PACKED_ICE.defaultBlockState());
                }
            }
        }
    }

    private static BlockState[] generateBands(RandomSource random) {
        BlockState[] bands = new BlockState[kBandCount];
        Arrays.fill(bands, Blocks.TERRACOTTA.defaultBlockState());
        for (int i = 0; i < bands.length; i++) {
            i += random.nextInt(5) + 1;
            if (i < bands.length) {
                bands[i] = Blocks.DYED_TERRACOTTA.orange().defaultBlockState();
            }
        }
        makeBands(random, bands, 1, Blocks.DYED_TERRACOTTA.yellow().defaultBlockState());
        makeBands(random, bands, 2, Blocks.DYED_TERRACOTTA.brown().defaultBlockState());
        makeBands(random, bands, 1, Blocks.DYED_TERRACOTTA.red().defaultBlockState());
        int whiteCount = random.nextIntBetweenInclusive(9, 15);
        BlockState white = Blocks.DYED_TERRACOTTA.white().defaultBlockState();
        BlockState gray = Blocks.DYED_TERRACOTTA.lightGray().defaultBlockState();
        int placed = 0;
        for (int at = 0; placed < whiteCount && at < bands.length; at += random.nextInt(16) + 4) {
            bands[at] = white;
            if (at - 1 > 0 && random.nextBoolean()) {
                bands[at - 1] = gray;
            }
            if (at + 1 < bands.length && random.nextBoolean()) {
                bands[at + 1] = gray;
            }
            placed++;
        }
        return bands;
    }

    private static void makeBands(RandomSource random, BlockState[] bands, int baseWidth, BlockState state) {
        int count = random.nextIntBetweenInclusive(6, 15);
        for (int i = 0; i < count; i++) {
            int width = baseWidth + random.nextInt(3);
            int start = random.nextInt(bands.length);
            for (int p = 0; start + p < bands.length && p < width; p++) {
                bands[start + p] = state;
            }
        }
    }

    protected BlockState getBand(int worldX, int y, int worldZ) {
        int offset = Math.round(clayBandsOffsetNoise.get(worldX, 0.0, worldZ) * 4.0F);
        return clayBands[(y + offset + clayBands.length) % clayBands.length];
    }

    public DensityFunction preliminarySurfaceFunction() {
        return preliminarySurfaceFunction;
    }
}
