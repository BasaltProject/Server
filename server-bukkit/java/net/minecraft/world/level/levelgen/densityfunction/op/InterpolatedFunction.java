// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record InterpolatedFunction(DensityFunction input, int cellSizeXz, int cellSizeY) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:interpolated\",\"input\":" + JsonDensityFunction.of(input) + ",\"cell_size_xz\":" + cellSizeXz + ",\"cell_size_y\":" + cellSizeY + "}";
    }
    public static final MapCodec<InterpolatedFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.InterpolatedFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new Sampler(input.compileSampler(context), cellSizeXz, cellSizeY, 1.0F / (float) cellSizeXz, 1.0F / (float) cellSizeY);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction rewritten = rule.rewrite(input);
        return rewritten == input ? this : new InterpolatedFunction(rewritten, cellSizeXz, cellSizeY);
    }
    public Interval range() {
        return input.range();
    }
    public int domainAxes() {
        return input.domainAxes();
    }
    public MapCodec<InterpolatedFunction> codec() {
        return CODEC;
    }
    static record Sampler(DensitySampler input, int cellSizeXz, int cellSizeY, float cellSizeXzInv, float cellSizeYInv) implements DensitySampler {
        private void interpolate(SamplerContext context, DensityBuffer out, DensityVolume volume) {
            int firstX = Mth.floorDiv(volume.minBlockX(), cellSizeXz);
            int firstY = Mth.floorDiv(volume.minBlockY(), cellSizeY);
            int firstZ = Mth.floorDiv(volume.minBlockZ(), cellSizeXz);
            int cellsX = Mth.floorDiv(volume.maxBlockX(), cellSizeXz) - firstX + 1;
            int cellsY = Mth.floorDiv(volume.maxBlockY(), cellSizeY) - firstY + 1;
            int cellsZ = Mth.floorDiv(volume.maxBlockZ(), cellSizeXz) - firstZ + 1;
            DensityVolume grid = new DensityVolume(
                Math.floorMod(volume.maxBlockX(), cellSizeXz) == 0 ? cellsX : cellsX + 1,
                Math.floorMod(volume.maxBlockY(), cellSizeY) == 0 ? cellsY : cellsY + 1,
                Math.floorMod(volume.maxBlockZ(), cellSizeXz) == 0 ? cellsZ : cellsZ + 1,
                firstX * cellSizeXz, firstY * cellSizeY, firstZ * cellSizeXz, cellSizeXz, cellSizeY, cellSizeXz);
            try (ScopedDensityBuffer corners = context.acquireBuffer(grid)) {
                input.sampleVolume(context, corners, grid);
                for (int cz = 0; cz < cellsZ; cz++) {
                    int nz = Math.min(cz + 1, grid.sizeZ() - 1);
                    for (int cx = 0; cx < cellsX; cx++) {
                        int nx = Math.min(cx + 1, grid.sizeX() - 1);
                        for (int cy = 0; cy < cellsY; cy++) {
                            int ny = Math.min(cy + 1, grid.sizeY() - 1);
                            float[] c = {
                                corners.get(grid.indexUnchecked(cx, cy, cz)), corners.get(grid.indexUnchecked(nx, cy, cz)),
                                corners.get(grid.indexUnchecked(cx, ny, cz)), corners.get(grid.indexUnchecked(nx, ny, cz)),
                                corners.get(grid.indexUnchecked(cx, cy, nz)), corners.get(grid.indexUnchecked(nx, cy, nz)),
                                corners.get(grid.indexUnchecked(cx, ny, nz)), corners.get(grid.indexUnchecked(nx, ny, nz))};
                            fillCell(out, volume, grid, cx, cy, cz, c);
                        }
                    }
                }
            }
        }
        private void fillCell(DensityBuffer out, DensityVolume volume, DensityVolume grid, int cx, int cy, int cz, float[] c) {
            int baseX = grid.blockX(cx) - volume.minBlockX();
            int baseY = grid.blockY(cy) - volume.minBlockY();
            int baseZ = grid.blockZ(cz) - volume.minBlockZ();
            int fromX = Math.max(0, -baseX);
            int fromY = Math.max(0, -baseY);
            int fromZ = Math.max(0, -baseZ);
            int toX = Math.min(cellSizeXz, volume.sizeX() - baseX) - 1;
            int toY = Math.min(cellSizeY, volume.sizeY() - baseY) - 1;
            int toZ = Math.min(cellSizeXz, volume.sizeZ() - baseZ) - 1;
            for (int z = fromZ; z <= toZ; z++) {
                float az = (float) z * cellSizeXzInv;
                float lowFront = Mth.lerp(az, c[0], c[4]);
                float highFront = Mth.lerp(az, c[2], c[6]);
                float lowBack = Mth.lerp(az, c[1], c[5]);
                float highBack = Mth.lerp(az, c[3], c[7]);
                for (int x = fromX; x <= toX; x++) {
                    float ax = (float) x * cellSizeXzInv;
                    float bottom = Mth.lerp(ax, lowFront, lowBack);
                    float top = Mth.lerp(ax, highFront, highBack);
                    float slope = (top - bottom) * cellSizeYInv;
                    float value = bottom + slope * (float) fromY;
                    int at = volume.indexUnchecked(baseX + x, baseY + fromY, baseZ + z);
                    for (int y = fromY; y <= toY; y++) {
                        out.set(at++, value);
                        value += slope;
                    }
                }
            }
        }

        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        boolean onGrid = (volume.stepBlockX() == cellSizeXz || volume.sizeX() == 1) && (volume.stepBlockY() == cellSizeY || volume.sizeY() == 1)
            && (volume.stepBlockZ() == cellSizeXz || volume.sizeZ() == 1) && Math.floorMod(volume.minBlockX(), cellSizeXz) == 0
            && Math.floorMod(volume.minBlockY(), cellSizeY) == 0 && Math.floorMod(volume.minBlockZ(), cellSizeXz) == 0;
        if (onGrid) {
            input.sampleVolume(context, outputBuffer, volume);
        } else if (volume.stepBlockX() == 1 && volume.stepBlockY() == 1 && volume.stepBlockZ() == 1) {
            interpolate(context, outputBuffer, volume);
        } else {
            DensityVolume dense = new DensityVolume(volume.sizeX() * volume.stepBlockX(), volume.sizeY() * volume.stepBlockY(), volume.sizeZ() * volume.stepBlockZ(),
                volume.minBlockX(), volume.minBlockY(), volume.minBlockZ(), 1, 1, 1);
            try (ScopedDensityBuffer full = context.acquireBuffer(dense)) {
                interpolate(context, full, dense);
                for (int z = 0; z < volume.sizeZ(); z++) {
                    for (int x = 0; x < volume.sizeX(); x++) {
                        for (int y = 0; y < volume.sizeY(); y++) {
                            outputBuffer.set(volume.indexUnchecked(x, y, z),
                                full.get(dense.indexUnchecked(x * volume.stepBlockX(), y * volume.stepBlockY(), z * volume.stepBlockZ())));
                        }
                    }
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        int inX = Math.floorMod(blockX, cellSizeXz);
        int inY = Math.floorMod(blockY, cellSizeY);
        int inZ = Math.floorMod(blockZ, cellSizeXz);
        if (inX == 0 && inY == 0 && inZ == 0) {
            return input.sampleValue(context, blockX, blockY, blockZ);
        }
        DensityVolume corners = new DensityVolume(2, 2, 2, blockX - inX, blockY - inY, blockZ - inZ, cellSizeXz, cellSizeY, cellSizeXz);
        try (ScopedDensityBuffer values = context.acquireBuffer(corners)) {
            input.sampleVolume(context, values, corners);
            return Mth.lerp3((float) inX / (float) cellSizeXz, (float) inY / (float) cellSizeY, (float) inZ / (float) cellSizeXz,
                values.get(corners.indexUnchecked(0, 0, 0)), values.get(corners.indexUnchecked(1, 0, 0)),
                values.get(corners.indexUnchecked(0, 1, 0)), values.get(corners.indexUnchecked(1, 1, 0)),
                values.get(corners.indexUnchecked(0, 0, 1)), values.get(corners.indexUnchecked(1, 0, 1)),
                values.get(corners.indexUnchecked(0, 1, 1)), values.get(corners.indexUnchecked(1, 1, 1)));
        }
        }
    }
}
