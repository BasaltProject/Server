// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
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
public record SteppedColumnClusterFeature(Holder<BlockStateProvider> block, BlockPredicate continueThrough, BlockPredicate canReplace, HolderSet<Block> cannotPlaceOn, IntProvider clusterReach, IntProvider columnCount, IntProvider columnReach, IntProvider height) implements Feature {
    private boolean standable(WorldGenLevel level, BlockPos.MutableBlockPos cursor) {
        if (!canReplace.test(level, cursor)) {
            return false;
        }
        BlockState under = level.getBlockState(cursor.move(Direction.DOWN));
        cursor.move(Direction.UP);
        return !under.isAir() && !under.is(cannotPlaceOn);
    }
    private BlockPos seekGround(WorldGenLevel level, BlockPos.MutableBlockPos cursor, int budget) {
        for (int left = budget; cursor.getY() > level.getMinY() + 1 && left > 0; left--) {
            if (standable(level, cursor)) {
                return cursor;
            }
            cursor.move(Direction.DOWN);
        }
        return null;
    }
    private BlockPos seekAir(LevelAccessor level, BlockPos.MutableBlockPos cursor, int budget) {
        for (int left = budget; cursor.getY() <= level.getMaxY() && left > 0; left--) {
            BlockState here = level.getBlockState(cursor);
            if (here.is(cannotPlaceOn)) {
                return null;
            }
            if (here.isAir()) {
                return cursor;
            }
            cursor.move(Direction.UP);
        }
        return null;
    }
    private boolean raiseColumn(WorldGenLevel level, RandomSource random, BlockPos base, int tall, int reach) {
        boolean any = false;
        for (BlockPos spot : BlockPos.betweenClosed(base.getX() - reach, base.getY(), base.getZ() - reach, base.getX() + reach, base.getY(), base.getZ() + reach)) {
            int dist = spot.distManhattan(base);
            BlockPos start = canReplace.test(level, spot) ? seekGround(level, spot.mutable(), dist) : seekAir(level, spot.mutable(), dist);
            if (start == null) {
                continue;
            }
            int blocks = tall - dist / 2;
            for (BlockPos.MutableBlockPos cursor = start.mutable(); blocks >= 0; blocks--) {
                if (canReplace.test(level, cursor)) {
                    setBlock(level, cursor, block.value().getState(level, random, cursor));
                    cursor.move(Direction.UP);
                    any = true;
                } else if (continueThrough.test(level, cursor)) {
                    cursor.move(Direction.UP);
                } else {
                    break;
                }
            }
        }
        return any;
    }
    public static final MapCodec<SteppedColumnClusterFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SteppedColumnClusterFeature.class, "CODEC");
    public MapCodec<SteppedColumnClusterFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        int seaLevel = chunkGenerator.getSeaLevel();
        if (!standable(level, origin.mutable())) {
            return false;
        }
        int tall = height.sample(random);
        int spread = Math.min(tall, clusterReach.sample(random));
        int count = columnCount.sample(random);
        boolean any = false;
        for (BlockPos spot : BlockPos.randomBetweenClosed(random, count, origin.getX() - spread, origin.getY(), origin.getZ() - spread,
            origin.getX() + spread, origin.getY(), origin.getZ() + spread)) {
            int blocks = tall - spot.distManhattan(origin);
            if (blocks >= 0) {
                any |= raiseColumn(level, random, spot, blocks, columnReach.sample(random));
            }
        }
        return any;
    }
}
