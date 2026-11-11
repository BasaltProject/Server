// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Interval;
import net.minecraft.util.Mth;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.levelgen.structure.TerrainAdjustment;
import net.minecraft.world.level.levelgen.structure.PoolElementStructurePiece;
import net.minecraft.world.level.levelgen.structure.pools.JigsawJunction;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;

public class Beardifier implements DensitySampler {
    public static final Interval RANGE = Interval.INFINITE;
    public static final ContextKey<Beardifier> CONTEXT_KEY = ContextKey.vanilla("beardifier");
    public static final int BEARD_KERNEL_RADIUS = 12;
    private static final int kKernelSize = 24;
    private static final float kMaxBuryDistance = 6.0F;
    private static final float[] kKernel = buildKernel();
    public static final Beardifier EMPTY = new Beardifier(List.of(), List.of(), null);
    private final List<Rigid> pieces;
    private final List<JigsawJunction> junctions;
    private final BoundingBox affectedBox;

    public static Beardifier forStructuresInChunk(StructureManager structureManager, ChunkPos chunkPos) {
        List<StructureStart> starts = structureManager.startsForStructure(chunkPos.x(), chunkPos.z(),
            structure -> structure.terrainAdaptation() != TerrainAdjustment.NONE);
        if (starts.isEmpty()) {
            return EMPTY;
        }
        int minX = chunkPos.getMinBlockX();
        int minZ = chunkPos.getMinBlockZ();
        List<Rigid> rigids = new ArrayList<>();
        List<JigsawJunction> joints = new ArrayList<>();
        BoundingBox union = null;
        for (StructureStart start : starts) {
            TerrainAdjustment adjustment = start.getStructure().terrainAdaptation();
            for (StructurePiece piece : start.getPieces()) {
                if (!piece.isCloseToChunk(chunkPos, BEARD_KERNEL_RADIUS)) {
                    continue;
                }
                if (!(piece instanceof PoolElementStructurePiece pooled)) {
                    rigids.add(new Rigid(piece.getBoundingBox(), adjustment, 0));
                    union = merge(union, piece.getBoundingBox());
                    continue;
                }
                if (pooled.getElement().getProjection() == StructureTemplatePool.Projection.RIGID) {
                    rigids.add(new Rigid(pooled.getBoundingBox(), adjustment, pooled.getGroundLevelDelta()));
                    union = merge(union, piece.getBoundingBox());
                }
                for (JigsawJunction junction : pooled.getJunctions()) {
                    int jx = junction.getSourceX();
                    int jz = junction.getSourceZ();
                    boolean near = jx > minX - BEARD_KERNEL_RADIUS && jz > minZ - BEARD_KERNEL_RADIUS
                        && jx < minX + 15 + BEARD_KERNEL_RADIUS && jz < minZ + 15 + BEARD_KERNEL_RADIUS;
                    if (near) {
                        joints.add(junction);
                        union = merge(union, new BoundingBox(new BlockPos(jx, junction.getSourceGroundY(), jz)));
                    }
                }
            }
        }
        if (union == null) {
            return EMPTY;
        }
        return new Beardifier(List.copyOf(rigids), List.copyOf(joints), union.inflatedBy(kKernelSize));
    }

    private static BoundingBox merge(BoundingBox current, BoundingBox extra) {
        return current == null ? extra : BoundingBox.encapsulating(current, extra);
    }

    public Beardifier(List<Rigid> pieces, List<JigsawJunction> junctions, BoundingBox affectedBox) {
        this.pieces = pieces;
        this.junctions = junctions;
        this.affectedBox = affectedBox;
    }

    public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        outputBuffer.fill(0.0F);
        if (affectedBox == null || !volume.intersects(affectedBox)) {
            return;
        }
        int x0 = Math.floorDiv(Math.max(0, affectedBox.minX() - volume.minBlockX()), volume.stepBlockX());
        int y0 = Math.floorDiv(Math.max(0, affectedBox.minY() - volume.minBlockY()), volume.stepBlockY());
        int z0 = Math.floorDiv(Math.max(0, affectedBox.minZ() - volume.minBlockZ()), volume.stepBlockZ());
        int x1 = Math.min(volume.sizeX() - 1,
            Math.floorDiv(affectedBox.maxX() - volume.minBlockX(), volume.stepBlockX()));
        int y1 = Math.min(volume.sizeY() - 1,
            Math.floorDiv(affectedBox.maxY() - volume.minBlockY(), volume.stepBlockY()));
        int z1 = Math.min(volume.sizeZ() - 1,
            Math.floorDiv(affectedBox.maxZ() - volume.minBlockZ(), volume.stepBlockZ()));
        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                for (int y = y0; y <= y1; y++) {
                    outputBuffer.set(volume.indexUnchecked(x, y, z),
                        densityAt(volume.blockX(x), volume.blockY(y), volume.blockZ(z)));
                }
            }
        }
    }

    public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        boolean inside = affectedBox != null && affectedBox.isInside(blockX, blockY, blockZ);
        return inside ? densityAt(blockX, blockY, blockZ) : 0.0F;
    }

    private float densityAt(int blockX, int blockY, int blockZ) {
        float total = 0.0F;
        for (Rigid rigid : pieces) {
            BoundingBox box = rigid.box();
            int dx = Math.max(0, Math.max(box.minX() - blockX, blockX - box.maxX()));
            int dz = Math.max(0, Math.max(box.minZ() - blockZ, blockZ - box.maxZ()));
            int ground = box.minY() + rigid.groundLevelDelta();
            int toGround = blockY - ground;
            switch (rigid.terrainAdjustment()) {
                case BURY -> total += buryContribution(dx, toGround / 2.0F, dz);
                case BEARD_THIN -> total += beardContribution(dx, toGround, dz, toGround) * 0.8F;
                case BEARD_BOX -> total += beardContribution(dx,
                    Math.max(0, Math.max(ground - blockY, blockY - box.maxY())), dz, toGround) * 0.8F;
                case ENCAPSULATE -> {
                    int dy = Math.max(0, Math.max(box.minY() - blockY, blockY - box.maxY()));
                    total += buryContribution(dx / 2.0F, dy / 2.0F, dz / 2.0F) * 0.8F;
                }
                default -> {
                }
            }
        }
        for (JigsawJunction junction : junctions) {
            int dy = blockY - junction.getSourceGroundY();
            total += beardContribution(blockX - junction.getSourceX(), dy, blockZ - junction.getSourceZ(), dy) * 0.4F;
        }
        return total;
    }

    private static float buryContribution(float dx, float dy, float dz) {
        float squared = Mth.lengthSquared(dx, dy, dz);
        return squared >= kMaxBuryDistance * kMaxBuryDistance ? 0.0F : 1.0F - Mth.sqrt(squared) / kMaxBuryDistance;
    }

    private static float beardContribution(int dx, int dy, int dz, int yToGround) {
        int xi = dx + BEARD_KERNEL_RADIUS;
        int yi = dy + BEARD_KERNEL_RADIUS;
        int zi = dz + BEARD_KERNEL_RADIUS;
        if (!inKernel(xi) || !inKernel(yi) || !inKernel(zi)) {
            return 0.0F;
        }
        float shifted = yToGround + 0.5F;
        float squared = Mth.lengthSquared((float) dx, shifted, (float) dz);
        float value = -shifted * (float) Mth.fastInvSqrt(squared / 2.0F) / 2.0F;
        return value * kKernel[zi * kKernelSize * kKernelSize + xi * kKernelSize + yi];
    }

    private static boolean inKernel(int index) {
        return index >= 0 && index < kKernelSize;
    }

    private static float[] buildKernel() {
        float[] kernel = new float[kKernelSize * kKernelSize * kKernelSize];
        for (int zi = 0; zi < kKernelSize; zi++) {
            for (int xi = 0; xi < kKernelSize; xi++) {
                for (int yi = 0; yi < kKernelSize; yi++) {
                    double squared = Mth.lengthSquared((double) (xi - BEARD_KERNEL_RADIUS), yi - BEARD_KERNEL_RADIUS + 0.5,
                        (double) (zi - BEARD_KERNEL_RADIUS));
                    kernel[zi * kKernelSize * kKernelSize + xi * kKernelSize + yi] = (float) Math.pow(Math.E, -squared / 16.0);
                }
            }
        }
        return kernel;
    }

    public static record Rigid(BoundingBox box, TerrainAdjustment terrainAdjustment, int groundLevelDelta) {
    }
}
