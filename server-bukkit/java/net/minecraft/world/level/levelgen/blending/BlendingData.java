// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blending;

import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.CompositeDirection;
import net.minecraft.core.Holder;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
public class BlendingData {
    private static final int kQuartsPerSection = 4;
    private static final int kInsideMax = kQuartsPerSection - 1;
    private static final int kOutsideMax = kQuartsPerSection;
    private static final int kInsideCount = 2 * kInsideMax + 1;
    private static final int kColumnCount = kInsideCount + 2 * kOutsideMax + 1;
    protected static final int CELL_WIDTH = 4;
    protected static final int CELL_HEIGHT = 8;
    protected static final int CELL_RATIO = 2;
    protected static final float NO_VALUE = Float.MAX_VALUE;
    private final LevelHeightAccessor area;
    private final float[] heights;
    private final float[][] densities = new float[kColumnCount][];
    private final java.util.List<java.util.List<Holder<Biome>>> biomes = new java.util.ArrayList<>(java.util.Collections.nCopies(kColumnCount, null));

    protected BlendingData() {
        this(0, 0, Optional.empty());
    }

    private BlendingData(int minSection, int maxSection, Optional<float[]> heights) {
        this.heights = heights.orElseGet(() -> {
            float[] empty = new float[kColumnCount];
            java.util.Arrays.fill(empty, NO_VALUE);
            return empty;
        });
        this.area = LevelHeightAccessor.create(minSection * 16, (maxSection - minSection) * 16);
    }

    public static BlendingData unpack(Packed packed) {
        return packed == null ? null : new BlendingData(packed.minSection(), packed.maxSection(), packed.heights());
    }

    public Packed pack() {
        boolean any = false;
        for (float height : heights) {
            any |= height != NO_VALUE;
        }
        return new Packed(area.getMinSectionY(), area.getMaxSectionY() + 1, any ? Optional.of(heights.clone()) : Optional.empty());
    }

    public static BlendingData getOrUpdateBlendingData(WorldGenRegion region, int chunkX, int chunkZ) {
        net.minecraft.world.level.chunk.ChunkAccess chunk = region.getChunk(chunkX, chunkZ);
        BlendingData data = chunk.getBlendingData();
        return data != null && !chunk.getHighestGeneratedStatus().isBefore(net.minecraft.world.level.chunk.status.ChunkStatus.BIOMES) ? data : null;
    }

    public static Set<CompositeDirection.Direction8> sideByGenerationAge(WorldGenLevel region, int chunkX, int chunkZ, boolean wantedOldGen) {
        Set<CompositeDirection.Direction8> sides = java.util.EnumSet.noneOf(CompositeDirection.Direction8.class);
        for (CompositeDirection.Direction8 direction : CompositeDirection.Direction8.values()) {
            if (region.getChunk(chunkX + direction.getStepX(), chunkZ + direction.getStepZ()).isOldNoiseGeneration() == wantedOldGen) {
                sides.add(direction);
            }
        }
        return sides;
    }

    private static int insideIndex(int x, int z) {
        return kInsideMax - x + z;
    }

    private static int outsideIndex(int x, int z) {
        return kInsideCount + x + kOutsideMax - z;
    }

    private static int cellX(int index) {
        if (index < kInsideCount) {
            return Math.max(0, kInsideMax - index);
        }
        return kOutsideMax - Math.max(0, kOutsideMax - (index - kInsideCount));
    }

    private static int cellZ(int index) {
        if (index < kInsideCount) {
            return Math.max(0, index - kInsideMax);
        }
        return kOutsideMax - Math.max(0, index - kInsideCount - kOutsideMax);
    }

    private int columnMinCell() {
        return area.getMinSectionY() * 2 + 1;
    }

    private int cellCount() {
        return area.getSectionsCount() * 2;
    }

    protected float getHeight(int cellX, int cellY, int cellZ) {
        if (cellX == kOutsideMax || cellZ == kOutsideMax) {
            return heights[outsideIndex(cellX, cellZ)];
        }
        return cellX != 0 && cellZ != 0 ? NO_VALUE : heights[insideIndex(cellX, cellZ)];
    }

    private float density(float[] column, int cellY) {
        if (column == null) {
            return NO_VALUE;
        }
        int index = cellY - columnMinCell();
        return index >= 0 && index < column.length ? column[index] * 0.1F : NO_VALUE;
    }

    protected float getDensity(int cellX, int cellY, int cellZ) {
        if (cellY == area.getMinSectionY() * 2) {
            return 0.1F;
        }
        if (cellX == kOutsideMax || cellZ == kOutsideMax) {
            return density(densities[outsideIndex(cellX, cellZ)], cellY);
        }
        return cellX != 0 && cellZ != 0 ? NO_VALUE : density(densities[insideIndex(cellX, cellZ)], cellY);
    }

    protected void iterateBiomes(int minCellX, int quartY, int minCellZ, BiomeConsumer biomeConsumer) {
        int minQuart = net.minecraft.core.QuartPos.fromBlock(area.getMinY());
        if (quartY < minQuart || quartY > net.minecraft.core.QuartPos.fromBlock(area.getMaxY())) {
            return;
        }
        for (int i = 0; i < biomes.size(); i++) {
            java.util.List<Holder<Biome>> column = biomes.get(i);
            Holder<Biome> biome = column == null ? null : column.get(quartY - minQuart);
            if (biome != null) {
                biomeConsumer.consume(minCellX + cellX(i), minCellZ + cellZ(i), biome);
            }
        }
    }

    protected void iterateHeights(int minCellX, int minCellZ, HeightConsumer heightConsumer) {
        for (int i = 0; i < heights.length; i++) {
            if (heights[i] != NO_VALUE) {
                heightConsumer.consume(minCellX + cellX(i), minCellZ + cellZ(i), heights[i]);
            }
        }
    }

    protected void iterateDensities(int minCellX, int minCellZ, int fromCellY, int toCellY, DensityConsumer densityConsumer) {
        int minCell = columnMinCell();
        int from = Math.max(0, fromCellY - minCell);
        int to = Math.min(cellCount(), toCellY - minCell);
        for (int i = 0; i < densities.length; i++) {
            if (densities[i] != null) {
                for (int y = from; y < to; y++) {
                    densityConsumer.consume(minCellX + cellX(i), y + minCell, minCellZ + cellZ(i), densities[i][y] * 0.1F);
                }
            }
        }
    }

    public LevelHeightAccessor getAreaWithOldGeneration() {
        return area;
    }
    public static record Packed(int minSection, int maxSection, Optional<float[]> heights) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.blending.BlendingData.Packed.class, "CODEC");
    }
    protected interface BiomeConsumer {
        void consume(int cellX, int cellZ, Holder<Biome> biome);
    }
    protected interface DensityConsumer {
        void consume(int cellX, int cellY, int cellZ, float density);
    }
    protected interface HeightConsumer {
        void consume(int cellX, int cellZ, float height);
    }
}
