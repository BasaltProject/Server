// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
public record BlockBlobFeature(BlockState state, BlockPredicate canPlaceOn) implements Feature {
    public static final MapCodec<BlockBlobFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.BlockBlobFeature.class, "CODEC");
    public MapCodec<BlockBlobFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos center = origin;
        while (center.getY() > level.getMinY() + 3 && !canPlaceOn.test(level, center.below())) {
            center = center.below();
        }
        if (center.getY() <= level.getMinY() + 3) {
            return false;
        }
        for (int round = 0; round < 3; round++) {
            int rx = random.nextInt(2);
            int ry = random.nextInt(2);
            int rz = random.nextInt(2);
            float reach = (float) (rx + ry + rz) * 0.333F + 0.5F;
            for (BlockPos cell : BlockPos.betweenClosed(center.offset(-rx, -ry, -rz), center.offset(rx, ry, rz))) {
                if (cell.distSqr(center) <= (double) (reach * reach)) {
                    level.setBlockAndUpdate(cell, state);
                }
            }
            center = center.offset(-1 + random.nextInt(2), -random.nextInt(2), -1 + random.nextInt(2));
        }
        return true;
    }
}
