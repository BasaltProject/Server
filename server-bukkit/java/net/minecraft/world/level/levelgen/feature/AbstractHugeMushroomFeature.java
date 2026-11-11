// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public interface AbstractHugeMushroomFeature extends Feature {
    int MIN_MUSHROOM_HEIGHT = 0;
    Holder<BlockStateProvider> capProvider();
    Holder<BlockStateProvider> stemProvider();
    int foliageRadius();
    BlockPredicate canPlaceOn();
    MapCodec<? extends AbstractHugeMushroomFeature> codec();
    default void placeTrunk(WorldGenLevel level, RandomSource random, BlockPos origin, int treeHeight, BlockPos.MutableBlockPos blockPos) {
        for (int up = 0; up < treeHeight; up++) {
            blockPos.set(origin).move(Direction.UP, up);
            placeMushroomBlock(level, blockPos, stemProvider().value().getState(level, random, origin));
        }
    }
    default void placeMushroomBlock(LevelAccessor level, BlockPos.MutableBlockPos blockPos, BlockState newState) {
        BlockState present = level.getBlockState(blockPos);
        if (present.isAir() || present.is(net.minecraft.tags.BlockTags.REPLACEABLE_BY_MUSHROOMS)) {
            setBlock(level, blockPos, newState);
        }
    }
    default int getTreeHeight(RandomSource random) {
        int height = random.nextInt(3) + 4;
        if (random.nextInt(12) == 0) {
            height *= 2;
        }
        return height;
    }
    default boolean isValidPosition(WorldGenLevel level, BlockPos origin, int treeHeight, BlockPos.MutableBlockPos blockPos) {
        int y = origin.getY();
        if (y < level.getMinY() + 1 || y + treeHeight + 1 > level.getMaxY()) {
            return false;
        }
        if (!canPlaceOn().test(level, origin.below())) {
            return false;
        }
        for (int dy = 0; dy <= treeHeight; dy++) {
            int reach = getTreeRadiusForHeight(-1, -1, foliageRadius(), dy);
            for (int dx = -reach; dx <= reach; dx++) {
                for (int dz = -reach; dz <= reach; dz++) {
                    BlockState found = level.getBlockState(blockPos.setWithOffset(origin, dx, dy, dz));
                    if (!found.isAir() && !found.is(net.minecraft.tags.BlockTags.LEAVES)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    default boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        int height = getTreeHeight(random);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        if (!isValidPosition(level, origin, height, cursor)) {
            return false;
        }
        makeCap(level, random, origin, height, cursor);
        placeTrunk(level, random, origin, height, cursor);
        return true;
    }
    int getTreeRadiusForHeight(int trunkHeight, int treeHeight, int leafRadius, int yo);
    void makeCap(WorldGenLevel level, RandomSource random, BlockPos origin, int treeHeight, BlockPos.MutableBlockPos blockPos);
}
