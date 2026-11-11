// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleTest;
public class OreFeature extends AbstractOreFeature {
    public static final MapCodec<OreFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.OreFeature.class, "CODEC");
    public OreFeature(List<BlockReplacement> targetStates, int size, float discardChanceOnAirExposure) {
        super(targetStates, size, discardChanceOnAirExposure);
    }
    public OreFeature(List<BlockReplacement> targetStates, int size) {
        this(targetStates, size, 0.0F);
    }
    public OreFeature(RuleTest target, BlockState state, int size) {
        this(List.of(new BlockReplacement(target, state)), size, 0.0F);
    }
    public MapCodec<OreFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        float angle = random.nextFloat() * (float) Math.PI;
        float spreadXZ = (float) size / 8.0F;
        int pad = Mth.ceil(((float) size / 16.0F * 2.0F + 1.0F) / 2.0F);
        double ax = (double) origin.getX() + Math.sin((double) angle) * (double) spreadXZ;
        double bx = (double) origin.getX() - Math.sin((double) angle) * (double) spreadXZ;
        double az = (double) origin.getZ() + Math.cos((double) angle) * (double) spreadXZ;
        double bz = (double) origin.getZ() - Math.cos((double) angle) * (double) spreadXZ;
        double ay = (double) (origin.getY() + random.nextInt(3) - 2);
        double by = (double) (origin.getY() + random.nextInt(3) - 2);
        int minX = origin.getX() - Mth.ceil(spreadXZ) - pad;
        int minY = origin.getY() - 2 - pad;
        int minZ = origin.getZ() - Mth.ceil(spreadXZ) - pad;
        int spanXZ = 2 * (Mth.ceil(spreadXZ) + pad);
        int spanY = 2 * (2 + pad);
        for (int x = minX; x <= minX + spanXZ; x++) {
            for (int z = minZ; z <= minZ + spanXZ; z++) {
                if (minY <= level.getHeight(Heightmap.Types.OCEAN_FLOOR_WG, x, z)) {
                    return doPlace(level, random, ax, bx, az, bz, ay, by, minX, minY, minZ, spanXZ, spanY);
                }
            }
        }
        return false;
    }
    protected boolean doPlace(WorldGenLevel level, RandomSource random, double x0, double x1, double z0, double z1, double y0, double y1, int xStart, int yStart, int zStart, int sizeXZ, int sizeY) {
        int placed = 0;
        java.util.BitSet seen = new java.util.BitSet(sizeXZ * sizeY * sizeXZ);
        BlockPos.MutableBlockPos spot = new BlockPos.MutableBlockPos();
        double[] blobs = new double[size * 4];
        for (int i = 0; i < size; i++) {
            float t = (float) i / (float) size;
            double bx = Mth.lerp((double) t, x0, x1);
            double by = Mth.lerp((double) t, y0, y1);
            double bz = Mth.lerp((double) t, z0, z1);
            double scale = random.nextDouble() * (double) size / 16.0D;
            double radius = ((double) (Mth.sin((double) ((float) Math.PI * t)) + 1.0F) * scale + 1.0D) / 2.0D;
            blobs[i * 4] = bx;
            blobs[i * 4 + 1] = by;
            blobs[i * 4 + 2] = bz;
            blobs[i * 4 + 3] = radius;
        }
        for (int i = 0; i < size - 1; i++) {
            if (blobs[i * 4 + 3] <= 0.0D) {
                continue;
            }
            for (int j = i + 1; j < size; j++) {
                if (blobs[j * 4 + 3] <= 0.0D) {
                    continue;
                }
                double dx = blobs[i * 4] - blobs[j * 4];
                double dy = blobs[i * 4 + 1] - blobs[j * 4 + 1];
                double dz = blobs[i * 4 + 2] - blobs[j * 4 + 2];
                double dr = blobs[i * 4 + 3] - blobs[j * 4 + 3];
                if (dr * dr > dx * dx + dy * dy + dz * dz) {
                    blobs[(dr > 0.0D ? j : i) * 4 + 3] = -1.0D;
                }
            }
        }
        for (int i = 0; i < size; i++) {
            double r = blobs[i * 4 + 3];
            if (r < 0.0D) {
                continue;
            }
            double cx = blobs[i * 4];
            double cy = blobs[i * 4 + 1];
            double cz = blobs[i * 4 + 2];
            int loX = Math.max(Mth.floor(cx - r), xStart);
            int loY = Math.max(Mth.floor(cy - r), yStart);
            int loZ = Math.max(Mth.floor(cz - r), zStart);
            int hiX = Math.max(Mth.floor(cx + r), loX);
            int hiY = Math.max(Mth.floor(cy + r), loY);
            int hiZ = Math.max(Mth.floor(cz + r), loZ);
            for (int x = loX; x <= hiX; x++) {
                double nx = ((double) x + 0.5D - cx) / r;
                if (nx * nx >= 1.0D) {
                    continue;
                }
                for (int y = loY; y <= hiY; y++) {
                    double ny = ((double) y + 0.5D - cy) / r;
                    if (nx * nx + ny * ny >= 1.0D) {
                        continue;
                    }
                    for (int z = loZ; z <= hiZ; z++) {
                        double nz = ((double) z + 0.5D - cz) / r;
                        if (nx * nx + ny * ny + nz * nz >= 1.0D || level.isOutsideBuildHeight(y)) {
                            continue;
                        }
                        int bit = x - xStart + (y - yStart) * sizeXZ + (z - zStart) * sizeXZ * sizeY;
                        if (seen.get(bit)) {
                            continue;
                        }
                        seen.set(bit);
                        spot.set(x, y, z);
                        if (!level.ensureCanWrite(spot)) {
                            continue;
                        }
                        BlockState current = level.getBlockState(spot);
                        for (BlockReplacement entry : targetStates) {
                            if (canPlaceOre(current, level::getBlockState, random, entry, spot)) {
                                level.setBlock(spot, entry.state(), 18);
                                placed++;
                                break;
                            }
                        }
                    }
                }
            }
        }
        return placed > 0;


    }
}
