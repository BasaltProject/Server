// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record RootSystemFeature(Holder<PlacedFeature> treeFeature, int requiredVerticalSpaceForTree, int levelTestDistance, int maxLevelDeviation, int rootRadius, HolderSet<Block> rootReplaceable, Holder<BlockStateProvider> rootStateProvider, int rootPlacementAttempts, int rootColumnMaxHeight, int hangingRootRadius, int hangingRootsVerticalSpan, Holder<BlockStateProvider> hangingRootStateProvider, int hangingRootPlacementAttempts, int allowedVerticalWaterForTree, BlockPredicate allowedTreePosition) implements Feature {
    private static boolean roomy(BlockState state, int height, int waterLimit) {
        return state.isAir() || (height + 1 <= waterLimit && state.getFluidState().is(net.minecraft.tags.FluidTags.WATER));
    }
    private boolean hasSpace(WorldGenLevel level, BlockPos pos) {
        BlockPos.MutableBlockPos up = pos.mutable();
        for (int i = 1; i <= requiredVerticalSpaceForTree; i++) {
            up.move(Direction.UP);
            if (!roomy(level.getBlockState(up), i, allowedVerticalWaterForTree)) {
                return false;
            }
        }
        if (levelTestDistance > 0) {
            BlockPos.MutableBlockPos corner = pos.mutable();
            for (int i = 0; i < 4; i++) {
                corner.move(Direction.from2DDataValue(i), levelTestDistance);
                BlockState under = level.getBlockState(corner.below(maxLevelDeviation));
                BlockState over = level.getBlockState(corner.above(maxLevelDeviation));
                if (under.isAir() || !over.isAir()) {
                    return false;
                }
                corner.set(pos);
            }
        }
        return true;
    }
    private void rootedDirt(WorldGenLevel level, RandomSource random, int x, int z, BlockPos.MutableBlockPos cursor) {
        for (int i = 0; i < rootPlacementAttempts; i++) {
            cursor.setWithOffset(cursor, random.nextInt(rootRadius) - random.nextInt(rootRadius), 0, random.nextInt(rootRadius) - random.nextInt(rootRadius));
            if (level.getBlockState(cursor).is(rootReplaceable)) {
                level.setBlock(cursor, rootStateProvider.value().getState(level, random, cursor), 2);
            }
            cursor.setX(x);
            cursor.setZ(z);
        }
    }
    private void dirtColumn(BlockPos origin, int top, WorldGenLevel level, RandomSource random) {
        BlockPos.MutableBlockPos cursor = origin.mutable();
        for (int y = origin.getY(); y < top; y++) {
            rootedDirt(level, random, origin.getX(), origin.getZ(), cursor.set(origin.getX(), y, origin.getZ()));
        }
    }
    private boolean growTree(WorldGenLevel level, ChunkGenerator generator, RandomSource random, BlockPos.MutableBlockPos cursor, BlockPos origin) {
        for (int y = 0; y < rootColumnMaxHeight; y++) {
            cursor.move(Direction.UP);
            if (level.getHeight(Heightmap.Types.WORLD_SURFACE, cursor) < cursor.getY()) {
                return false;
            }
            if (allowedTreePosition.test(level, cursor) && hasSpace(level, cursor)) {
                BlockPos under = cursor.below();
                if (level.getFluidState(under).is(net.minecraft.tags.FluidTags.LAVA) || !level.getBlockState(under).isSolid()) {
                    return false;
                }
                if (treeFeature.value().place(level, generator, random, cursor)) {
                    dirtColumn(origin, origin.getY() + y, level, random);
                    return true;
                }
            }
        }
        return false;
    }
    private void hangRoots(WorldGenLevel level, RandomSource random, BlockPos origin, BlockPos.MutableBlockPos cursor) {
        for (int i = 0; i < hangingRootPlacementAttempts; i++) {
            cursor.setWithOffset(origin, random.nextInt(hangingRootRadius) - random.nextInt(hangingRootRadius),
                random.nextInt(hangingRootsVerticalSpan) - random.nextInt(hangingRootsVerticalSpan),
                random.nextInt(hangingRootRadius) - random.nextInt(hangingRootRadius));
            if (level.isEmptyBlock(cursor)) {
                BlockState root = hangingRootStateProvider.value().getState(level, random, cursor);
                if (root.canSurvive(level, cursor) && level.getBlockState(cursor.above()).isFaceSturdy(level, cursor, Direction.DOWN)) {
                    level.setBlock(cursor, root, 2);
                }
            }
        }
    }
    public static final MapCodec<RootSystemFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.RootSystemFeature.class, "CODEC");
    public MapCodec<RootSystemFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!level.getBlockState(origin).isAir()) {
            return false;
        }
        BlockPos.MutableBlockPos cursor = origin.mutable();
        if (growTree(level, chunkGenerator, random, cursor, origin)) {
            hangRoots(level, random, origin, cursor);
        }
        return true;


    }
}
