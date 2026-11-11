// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record RandomNeighborSpreadFeature(Holder<BlockStateProvider> block, HolderSet<Block> acceptedNeighbors, BlockPredicate canReplace, IntProvider attempts, IntProvider xzOffset, IntProvider yOffset) implements Feature {
    public static final MapCodec<RandomNeighborSpreadFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.RandomNeighborSpreadFeature.class, "CODEC");
    public MapCodec<RandomNeighborSpreadFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        level.setBlock(origin, block.value().getState(level, random, origin), 2);
        BlockPos.MutableBlockPos target = new BlockPos.MutableBlockPos();
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        int tries = attempts.sample(random);
        for (int i = 0; i < tries; i++) {
            target.setWithOffset(origin, xzOffset.sample(random), yOffset.sample(random), xzOffset.sample(random));
            if (!canReplace.test(level, target)) {
                continue;
            }
            int touching = 0;
            for (Direction side : Direction.values()) {
                probe.setWithOffset(target, side);
                if (level.getBlockState(probe).is(acceptedNeighbors)) {
                    touching++;
                }
                if (touching > 1) {
                    break;
                }
            }
            if (touching == 1) {
                level.setBlock(target, block.value().getState(level, random, target), 2);
            }
        }
        return true;
    }
}
