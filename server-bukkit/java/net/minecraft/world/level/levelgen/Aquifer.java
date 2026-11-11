// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.longs.Long2IntMap;
import it.unimi.dsi.fastutil.longs.Long2IntOpenHashMap;
import java.util.Arrays;
import net.minecraft.core.BlockPos;
import net.minecraft.core.QuartPos;
import net.minecraft.core.SectionPos;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensitySamplerSet;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;

public interface Aquifer {
    static Aquifer createDisabled(FluidPicker fluidRule) {
        return new Aquifer() {
            public BlockState computeSubstance(int blockX, int blockY, int blockZ, double density) {
                return density > 0.0 ? null : fluidRule.computeFluid(blockX, blockY, blockZ).at(blockY);
            }

            public boolean shouldScheduleFluidUpdate() {
                return false;
            }
        };
    }

    BlockState computeSubstance(int blockX, int blockY, int blockZ, double density);

    boolean shouldScheduleFluidUpdate();

    public static record Config(DensityFunction barrierNoise, DensityFunction fluidLevelFloodednessNoise,
        DensityFunction fluidLevelSpreadNoise, DensityFunction lavaNoise, DensityFunction exclusion,
        DensityFunction surfaceLevel) {
        public static final Codec<Config> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Config.class, "CODEC");

        public Aquifer create(DensitySamplerSet cachingSamplers, PositionalRandomFactory positionalRandomFactory,
            DensityVolume volume, FluidPicker fluidRule) {
            return new NoiseBasedAquifer(cachingSamplers, this, positionalRandomFactory, volume, fluidRule);
        }
    }

    public static class NoiseBasedAquifer implements Aquifer {
        private static final int kCellHeight = 12;
        private static final int kWayBelowMinY = -2032 << 4;
        private static final double kFlowingSimilarity = similarity(100, 144);
        private static final int[][] kSurfaceOffsets = {{0, 0}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {-3, 0}, {-2, 0},
            {-1, 0}, {1, 0}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}};
        private final DensitySampler.Bound barrierNoise;
        private final DensitySampler.Bound floodednessNoise;
        private final DensitySampler.Bound spreadNoise;
        private final DensitySampler.Bound lavaNoise;
        private final DensitySampler.Bound exclusion;
        private final DensitySampler.Bound surfaceLevel;
        private final PositionalRandomFactory randomFactory;
        private final FluidPicker globalFluidPicker;
        private final FluidStatus[] statusCache;
        private final long[] locationCache;
        private final Long2IntMap surfaceCache = new Long2IntOpenHashMap();
        private final int skipSamplingAboveY;
        private final int minGridX;
        private final int minGridY;
        private final int minGridZ;
        private final int gridSizeX;
        private final int gridSizeZ;
        private boolean scheduleFluidUpdate;

        private NoiseBasedAquifer(DensitySamplerSet cachingSamplers, Config config,
            PositionalRandomFactory positionalRandomFactory, DensityVolume volume, FluidPicker globalFluidPicker) {
            java.util.function.Function<DensityFunction, DensitySampler.Bound> bind = cachingSamplers::get;
            this.barrierNoise = bind.apply(config.barrierNoise());
            this.floodednessNoise = cachingSamplers.get(config.fluidLevelFloodednessNoise());
            this.spreadNoise = cachingSamplers.get(config.fluidLevelSpreadNoise());
            this.lavaNoise = bind.apply(config.lavaNoise());
            this.exclusion = bind.apply(config.exclusion());
            this.surfaceLevel = bind.apply(config.surfaceLevel());
            this.randomFactory = positionalRandomFactory;
            this.globalFluidPicker = globalFluidPicker;
            this.minGridX = gridX(volume.minBlockX() - 5);
            int maxGridX = gridX(volume.maxBlockX() - 5) + 1;
            this.gridSizeX = maxGridX - minGridX + 1;
            this.minGridY = gridY(volume.minBlockY() + 1) - 1;
            int gridSizeY = gridY(volume.maxBlockY() + 1) + 1 - minGridY + 1;
            this.minGridZ = gridZ(volume.minBlockZ() - 5);
            int maxGridZ = gridZ(volume.maxBlockZ() - 5) + 1;
            this.gridSizeZ = maxGridZ - minGridZ + 1;
            int cells = gridSizeX * gridSizeY * gridSizeZ;
            this.statusCache = new FluidStatus[cells];
            this.locationCache = new long[cells];
            Arrays.fill(locationCache, Long.MAX_VALUE);
            int highest = adjustSurfaceLevel(maxSurfaceLevel(fromGridX(minGridX, 0), fromGridZ(minGridZ, 0),
                fromGridX(maxGridX, 9), fromGridZ(maxGridZ, 9)));
            this.skipSamplingAboveY = fromGridY(gridY(highest + 12) + 1, 11) - 1;
        }

        private int surfaceLevel(int blockX, int blockZ) {
            int qx = QuartPos.toBlock(QuartPos.fromBlock(blockX));
            int qz = QuartPos.toBlock(QuartPos.fromBlock(blockZ));
            long key = ChunkPos.pack(qx, qz);
            if (!surfaceCache.containsKey(key)) {
                surfaceCache.put(key, Mth.floor(surfaceLevel.sampleValue(qx, 0, qz)));
            }
            return surfaceCache.get(key);
        }

        private int maxSurfaceLevel(int minBlockX, int minBlockZ, int maxBlockX, int maxBlockZ) {
            int qMinX = QuartPos.fromBlock(minBlockX);
            int qMinZ = QuartPos.fromBlock(minBlockZ);
            DensityVolume grid = new DensityVolume(QuartPos.fromBlock(maxBlockX) - qMinX + 1, 1,
                QuartPos.fromBlock(maxBlockZ) - qMinZ + 1, QuartPos.toBlock(qMinX), 0, QuartPos.toBlock(qMinZ), 4, 1, 4);
            try (ScopedDensityBuffer buffer = surfaceLevel.sampleVolume(grid)) {
                int highest = Integer.MIN_VALUE;
                for (int z = 0; z < grid.sizeZ(); z++) {
                    for (int x = 0; x < grid.sizeX(); x++) {
                        int level = Mth.floor(buffer.get(grid.indexUnchecked(x, 0, z)));
                        surfaceCache.put(ChunkPos.pack(grid.blockX(x), grid.blockZ(z)), level);
                        highest = Math.max(highest, level);
                    }
                }
                return highest;
            }
        }

        private int cellIndex(int gx, int gy, int gz) {
            return ((gy - minGridY) * gridSizeZ + (gz - minGridZ)) * gridSizeX + (gx - minGridX);
        }

        private long locationOf(int gx, int gy, int gz, int index) {
            if (locationCache[index] == Long.MAX_VALUE) {
                RandomSource random = randomFactory.at(gx, gy, gz);
                int ox = random.nextInt(10);
                int oy = random.nextInt(9);
                int oz = random.nextInt(10);
                locationCache[index] = BlockPos.asLong(fromGridX(gx, ox), fromGridY(gy, oy), fromGridZ(gz, oz));
            }
            return locationCache[index];
        }

        public BlockState computeSubstance(int blockX, int blockY, int blockZ, double density) {
            scheduleFluidUpdate = false;
            if (density > 0.0) {
                return null;
            }
            FluidStatus global = globalFluidPicker.computeFluid(blockX, blockY, blockZ);
            if (blockY > skipSamplingAboveY) {
                return global.at(blockY);
            }
            if (global.at(blockY).is(Blocks.LAVA)) {
                return Blocks.LAVA.defaultBlockState();
            }
            int ax = gridX(blockX - 5);
            int ay = gridY(blockY + 1);
            int az = gridZ(blockZ - 5);
            int[] dist = {Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE};
            int[] nearest = new int[4];
            for (int dx = 0; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = 0; dz <= 1; dz++) {
                        int index = cellIndex(ax + dx, ay + dy, az + dz);
                        long where = locationOf(ax + dx, ay + dy, az + dz, index);
                        int ex = BlockPos.getX(where) - blockX;
                        int ey = BlockPos.getY(where) - blockY;
                        int ez = BlockPos.getZ(where) - blockZ;
                        insertNearest(dist, nearest, ex * ex + ey * ey + ez * ez, index);
                    }
                }
            }
            FluidStatus first = statusAt(nearest[0]);
            double sim12 = similarity(dist[0], dist[1]);
            BlockState fluid = first.at(blockY);
            if (sim12 <= 0.0) {
                scheduleFluidUpdate = sim12 >= kFlowingSimilarity && !first.equals(statusAt(nearest[1]));
                return fluid;
            }
            if (fluid.is(Blocks.WATER)
                && globalFluidPicker.computeFluid(blockX, blockY - 1, blockZ).at(blockY - 1).is(Blocks.LAVA)) {
                scheduleFluidUpdate = true;
                return fluid;
            }
            double[] barrier = {Double.NaN};
            FluidStatus second = statusAt(nearest[1]);
            if (density + sim12 * pressure(blockX, blockY, blockZ, barrier, first, second) > 0.0) {
                return null;
            }
            FluidStatus third = statusAt(nearest[2]);
            double sim13 = similarity(dist[0], dist[2]);
            if (sim13 > 0.0
                && density + sim12 * sim13 * pressure(blockX, blockY, blockZ, barrier, first, third) > 0.0) {
                return null;
            }
            double sim23 = similarity(dist[1], dist[2]);
            if (sim23 > 0.0
                && density + sim12 * sim23 * pressure(blockX, blockY, blockZ, barrier, second, third) > 0.0) {
                return null;
            }
            boolean flow12 = !first.equals(second);
            boolean flow23 = sim23 >= kFlowingSimilarity && !second.equals(third);
            boolean flow13 = sim13 >= kFlowingSimilarity && !first.equals(third);
            if (flow12 || flow23 || flow13) {
                scheduleFluidUpdate = true;
            } else {
                scheduleFluidUpdate = sim13 >= kFlowingSimilarity
                    && similarity(dist[0], dist[3]) >= kFlowingSimilarity && !first.equals(statusAt(nearest[3]));
            }
            return fluid;
        }

        private static void insertNearest(int[] dist, int[] nearest, int candidate, int index) {
            for (int slot = 0; slot < 4; slot++) {
                if (dist[slot] >= candidate) {
                    for (int move = 3; move > slot; move--) {
                        dist[move] = dist[move - 1];
                        nearest[move] = nearest[move - 1];
                    }
                    dist[slot] = candidate;
                    nearest[slot] = index;
                    return;
                }
            }
        }

        public boolean shouldScheduleFluidUpdate() {
            return scheduleFluidUpdate;
        }

        private static double similarity(int distanceSqr1, int distanceSqr2) {
            return 1.0 - (distanceSqr2 - distanceSqr1) / 25.0;
        }

        private double pressure(int blockX, int blockY, int blockZ, double[] barrierCache, FluidStatus a, FluidStatus b) {
            BlockState typeA = a.at(blockY);
            BlockState typeB = b.at(blockY);
            if (typeA.is(Blocks.LAVA) && typeB.is(Blocks.WATER) || typeA.is(Blocks.WATER) && typeB.is(Blocks.LAVA)) {
                return 2.0;
            }
            int levelGap = Math.abs(a.fluidLevel - b.fluidLevel);
            if (levelGap == 0) {
                return 0.0;
            }
            double above = blockY + 0.5 - 0.5 * (a.fluidLevel + b.fluidLevel);
            double toMiddle = levelGap / 2.0 - Math.abs(above);
            double gradient;
            if (above > 0.0) {
                gradient = toMiddle > 0.0 ? toMiddle / 1.5 : toMiddle / 2.5;
            } else {
                double centre = 3.0 + toMiddle;
                gradient = centre > 0.0 ? centre / 3.0 : centre / 10.0;
            }
            double noise = 0.0;
            if (gradient >= -2.0 && gradient <= 2.0) {
                if (Double.isNaN(barrierCache[0])) {
                    barrierCache[0] = barrierNoise.sampleValue(blockX, blockY, blockZ);
                }
                noise = barrierCache[0];
            }
            return 2.0 * (noise + gradient);
        }

        private static int gridX(int blockCoord) {
            return blockCoord >> 4;
        }

        private static int fromGridX(int gridCoord, int blockOffset) {
            return gridCoord * 16 + blockOffset;
        }

        private static int gridY(int blockCoord) {
            return Math.floorDiv(blockCoord, kCellHeight);
        }

        private static int fromGridY(int gridCoord, int blockOffset) {
            return blockOffset + gridCoord * kCellHeight;
        }

        private static int gridZ(int blockCoord) {
            return blockCoord >> 4;
        }

        private static int fromGridZ(int gridCoord, int blockOffset) {
            return gridCoord * 16 + blockOffset;
        }

        private FluidStatus statusAt(int index) {
            if (statusCache[index] == null) {
                long where = locationCache[index];
                statusCache[index] = fluidStatusFor(BlockPos.getX(where), BlockPos.getY(where), BlockPos.getZ(where));
            }
            return statusCache[index];
        }

        private FluidStatus fluidStatusFor(int x, int y, int z) {
            FluidStatus global = globalFluidPicker.computeFluid(x, y, z);
            int lowest = Integer.MAX_VALUE;
            int cellTop = y + 12;
            int cellBottom = y - 12;
            boolean centreUnderGlobal = false;
            for (int[] offset : kSurfaceOffsets) {
                int sampleX = SectionPos.sectionToBlockCoord(offset[0]) + x;
                int sampleZ = z + SectionPos.sectionToBlockCoord(offset[1]);
                int level = surfaceLevel(sampleX, sampleZ);
                int adjusted = adjustSurfaceLevel(level);
                boolean centre = offset[0] == 0 && offset[1] == 0;
                if (centre && cellBottom > adjusted) {
                    return global;
                }
                boolean poking = cellTop > adjusted;
                if (poking || centre) {
                    FluidStatus atSurface = globalFluidPicker.computeFluid(sampleX, adjusted, sampleZ);
                    if (!atSurface.at(adjusted).isAir()) {
                        centreUnderGlobal |= centre;
                        if (poking) {
                            return atSurface;
                        }
                    }
                }
                lowest = Math.min(lowest, level);
            }
            int fluidLevel = surfaceLevelFor(x, y, z, global, lowest, centreUnderGlobal);
            return new FluidStatus(fluidLevel, fluidTypeFor(x, y, z, global, fluidLevel));
        }

        private int adjustSurfaceLevel(int preliminarySurfaceLevel) {
            return preliminarySurfaceLevel + 8;
        }

        private int surfaceLevelFor(int x, int y, int z, FluidStatus global, int lowestSurface, boolean centreUnderGlobal) {
            double partial;
            double full;
            if (exclusion.sampleValue(x, y, z) > 0.0) {
                partial = -1.0;
                full = -1.0;
            } else {
                int below = adjustSurfaceLevel(lowestSurface) - y;
                double factor = centreUnderGlobal ? Mth.clampedMap((double) below, 0.0, 64.0, 1.0, 0.0) : 0.0;
                double noise = Mth.clamp((double) floodednessNoise.sampleValue(x, y, z), -1.0, 1.0);
                full = noise - Mth.map(factor, 1.0, 0.0, -0.3, 0.8);
                partial = noise - Mth.map(factor, 1.0, 0.0, -0.8, 0.4);
            }
            if (full > 0.0) {
                return global.fluidLevel;
            }
            return partial > 0.0 ? randomizedLevel(x, y, z, lowestSurface) : kWayBelowMinY;
        }

        private int randomizedLevel(int x, int y, int z, int lowestSurface) {
            int cellX = Math.floorDiv(x, 16);
            int cellY = Math.floorDiv(y, 40);
            int cellZ = Math.floorDiv(z, 16);
            double spread = spreadNoise.sampleValue(cellX, cellY, cellZ) * 10.0F;
            return Math.min(lowestSurface, cellY * 40 + 20 + Mth.quantize(spread, 3));
        }

        private BlockState fluidTypeFor(int x, int y, int z, FluidStatus global, int fluidLevel) {
            boolean candidate = fluidLevel <= -10 && fluidLevel != kWayBelowMinY
                && global.fluidType != Blocks.LAVA.defaultBlockState();
            if (candidate) {
                double lava = lavaNoise.sampleValue(Math.floorDiv(x, 64), Math.floorDiv(y, 40), Math.floorDiv(z, 64));
                if (Math.abs(lava) > 0.3) {
                    return Blocks.LAVA.defaultBlockState();
                }
            }
            return global.fluidType;
        }
    }

    public static record FluidStatus(int fluidLevel, BlockState fluidType) {
        public BlockState at(int blockY) {
            return blockY < fluidLevel ? fluidType : Blocks.AIR.defaultBlockState();
        }
    }

    public interface FluidPicker {
        FluidStatus computeFluid(int blockX, int blockY, int blockZ);
    }
}
