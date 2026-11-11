// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blending;

import java.util.Map;
import net.minecraft.core.CompositeDirection;
import net.minecraft.core.QuartPos;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.chunk.CarvingMask;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;

/** Basalt worlds are generated in one pass, so a blender never has old chunks to blend with. */
public class Blender {
    public static final ContextKey<Blender> CONTEXT_KEY = ContextKey.vanilla("blender");
    public static final ContextKey<DensitySampler> ALPHA_KEY = ContextKey.vanilla("blender/alpha");
    public static final ContextKey<DensitySampler> OFFSET_KEY = ContextKey.vanilla("blender/offset");
    private static final Blender kEmpty = new Blender(null);

    private final CarvingMask.Filter carvingFilter;

    public static Blender empty() {
        return kEmpty;
    }

    public static Blender of(WorldGenRegion region) {
        return kEmpty;
    }

    private Blender(CarvingMask.Filter carvingFilter) {
        this.carvingFilter = carvingFilter;
    }

    public boolean isEmpty() {
        return carvingFilter == null;
    }

    public OutputBuffer blendOffsetAndFactor(DensityVolume volume) {
        int minX = QuartPos.fromBlock(volume.minBlockX());
        int minZ = QuartPos.fromBlock(volume.minBlockZ());
        int countX = QuartPos.fromBlock(volume.maxBlockX()) + 2 - minX;
        int countZ = QuartPos.fromBlock(volume.maxBlockZ()) + 2 - minZ;
        float[] alphas = new float[countX * countZ];
        float[] offsets = new float[countX * countZ];
        for (int i = 0; i < alphas.length; i++) {
            BlendingOutput out = blendOffsetAndFactor(QuartPos.toBlock(minX + i % countX),
                QuartPos.toBlock(minZ + i / countX));
            alphas[i] = out.alpha;
            offsets[i] = out.blendingOffset;
        }
        return new OutputBuffer(this, minX, minZ, countX, countZ, alphas, offsets);
    }

    protected BlendingOutput blendOffsetAndFactor(int blockX, int blockZ) {
        return new BlendingOutput(1.0F, 0.0F);
    }

    public float blendDensity(int blockX, int blockY, int blockZ, float noiseValue) {
        return noiseValue;
    }

    public BiomeResolver getBiomeResolver(BiomeResolver biomeResolver) {
        return biomeResolver;
    }

    public static void generateBorderTicks(WorldGenRegion region, ChunkAccess chunk) {
    }

    public CarvingMask.Filter getCarvingFilter() {
        return carvingFilter;
    }

    public static DistanceGetter makeOldChunkDistanceGetter(BlendingData centerBlendingData,
        Map<CompositeDirection.Direction8, BlendingData> oldSidesBlendingData) {
        return (x, y, z) -> Double.POSITIVE_INFINITY;
    }

    protected static record BlendingOutput(float alpha, float blendingOffset) {
    }

    public static record OutputBuffer(Blender blender, int minCellX, int minCellZ, int cellCountX, int cellCountZ,
        float[] alphas, float[] offsets) {
        public DensitySampler createAlphaSampler() {
            return columnSampler(this::getAlpha);
        }

        public DensitySampler createOffsetSampler() {
            return columnSampler(this::getOffset);
        }

        private static DensitySampler columnSampler(ColumnValue value) {
            return new DensitySampler() {
                public void sampleVolume(SamplerContext context, DensityBuffer out, DensityVolume volume) {
                    DensitySampler.sampleVolumeNaive(context, out, volume, this);
                }

                public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
                    return value.at(blockX, blockZ);
                }
            };
        }

        public float getAlpha(int blockX, int blockZ) {
            int index = indexOf(blockX, blockZ);
            return index < 0 ? blender.blendOffsetAndFactor(blockX, blockZ).alpha() : alphas[index];
        }

        public float getOffset(int blockX, int blockZ) {
            int index = indexOf(blockX, blockZ);
            return index < 0 ? blender.blendOffsetAndFactor(blockX, blockZ).blendingOffset() : offsets[index];
        }

        private int indexOf(int blockX, int blockZ) {
            int x = QuartPos.fromBlock(blockX) - minCellX;
            int z = QuartPos.fromBlock(blockZ) - minCellZ;
            return x >= 0 && z >= 0 && x < cellCountX && z < cellCountZ ? x + z * cellCountX : -1;
        }

        private interface ColumnValue {
            float at(int blockX, int blockZ);
        }
    }

    interface CellValueGetter {
        float get(BlendingData data, int cellX, int cellY, int cellZ);
    }

    public interface DistanceGetter {
        double getDistance(double x, double y, double z);
    }
}
