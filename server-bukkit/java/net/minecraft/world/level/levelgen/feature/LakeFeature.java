// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record LakeFeature(Holder<BlockStateProvider> fluid, Holder<BlockStateProvider> barrier, BlockPredicate canPlaceFeature, BlockPredicate canReplaceWithAirOrFluid, BlockPredicate canReplaceWithBarrier) implements Feature {
    private static final BlockState kCaveAir = Blocks.CAVE_AIR.defaultBlockState();
    private static int slot(int x, int z, int y) {
        return (x * 16 + z) * 8 + y;
    }
    private static boolean edgeOf(boolean[] grid, int x, int z, int y) {
        return !grid[slot(x, z, y)]
            && (x < 15 && grid[slot(x + 1, z, y)] || x > 0 && grid[slot(x - 1, z, y)] || z < 15 && grid[slot(x, z + 1, y)]
                || z > 0 && grid[slot(x, z - 1, y)] || y < 7 && grid[slot(x, z, y + 1)] || y > 0 && grid[slot(x, z, y - 1)]);
    }
    public static final MapCodec<LakeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.LakeFeature.class, "CODEC");
    public MapCodec<LakeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (origin.getY() <= level.getMinY() + 4) {
            return false;
        }
        BlockPos base = origin.offset(-8, -4, -8);
        boolean[] grid = new boolean[2048];
        int blobs = random.nextInt(4) + 4;
        for (int n = 0; n < blobs; n++) {
            double sx = random.nextDouble() * 6.0D + 3.0D;
            double sy = random.nextDouble() * 4.0D + 2.0D;
            double sz = random.nextDouble() * 6.0D + 3.0D;
            double cx = random.nextDouble() * (16.0D - sx - 2.0D) + 1.0D + sx / 2.0D;
            double cy = random.nextDouble() * (8.0D - sy - 4.0D) + 2.0D + sy / 2.0D;
            double cz = random.nextDouble() * (16.0D - sz - 2.0D) + 1.0D + sz / 2.0D;
            for (int x = 1; x < 15; x++) {
                for (int z = 1; z < 15; z++) {
                    for (int y = 1; y < 7; y++) {
                        double nx = ((double) x - cx) / (sx / 2.0D);
                        double ny = ((double) y - cy) / (sy / 2.0D);
                        double nz = ((double) z - cz) / (sz / 2.0D);
                        if (nx * nx + ny * ny + nz * nz < 1.0D) {
                            grid[slot(x, z, y)] = true;
                        }
                    }
                }
            }
        }
        BlockState liquid = fluid.value().getState(level, random, base);
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                for (int y = 0; y < 8; y++) {
                    if (!edgeOf(grid, x, z, y)) {
                        continue;
                    }
                    BlockPos at = base.offset(x, y, z);
                    BlockState found = level.getBlockState(at);
                    if (y >= 4 && found.liquid()) {
                        return false;
                    }
                    if (y < 4 && !found.isSolid() && found != liquid) {
                        return false;
                    }
                    if (!canPlaceFeature.test(level, at)) {
                        return false;
                    }
                }
            }
        }
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                for (int y = 0; y < 8; y++) {
                    if (!grid[slot(x, z, y)]) {
                        continue;
                    }
                    BlockPos at = base.offset(x, y, z);
                    if (canReplaceWithAirOrFluid.test(level, at)) {
                        boolean dry = y >= 4;
                        level.setBlock(at, dry ? kCaveAir : liquid, 2);
                        if (dry) {
                            level.scheduleTick(at, kCaveAir.getBlock(), 0);
                            markAboveForPostProcessing(level, at);
                        }
                    }
                }
            }
        }
        BlockState wall = barrier.value().getState(level, random, base);
        if (!wall.isAir()) {
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    for (int y = 0; y < 8; y++) {
                        if (edgeOf(grid, x, z, y) && (y < 4 || random.nextInt(2) != 0)) {
                            BlockPos at = base.offset(x, y, z);
                            if (level.getBlockState(at).isSolid() && canReplaceWithBarrier.test(level, at)) {
                                level.setBlock(at, wall, 2);
                                markAboveForPostProcessing(level, at);
                            }
                        }
                    }
                }
            }
        }
        if (liquid.getFluidState().is(net.minecraft.tags.FluidTags.WATER)) {
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    BlockPos at = base.offset(x, 4, z);
                    if (level.getBiome(at).value().shouldFreeze(level, at, false) && canReplaceWithAirOrFluid.test(level, at)) {
                        level.setBlock(at, Blocks.ICE.defaultBlockState(), 2);
                    }
                }
            }
        }
        return true;


    }
}
