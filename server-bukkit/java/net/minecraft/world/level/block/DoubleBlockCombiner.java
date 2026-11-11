// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.function.BiPredicate;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;
public class DoubleBlockCombiner {
    public static <S extends BlockEntity> NeighborCombineResult<S> combineWithNeigbour(BlockEntityType<S> entityType, Function<BlockState, BlockType> typeResolver, Function<BlockState, Direction> connectionResolver, Property<Direction> facingProperty, BlockState state, LevelAccessor level, BlockPos pos, BiPredicate<LevelAccessor, BlockPos> blockedChecker) {
        S blockEntity = entityType.getBlockEntity(level, pos);
        if (blockEntity == null || blockedChecker.test(level, pos)) {
            return Combiner::acceptNone;
        }
        BlockType type = typeResolver.apply(state);
        if (type == BlockType.SINGLE) {
            return new NeighborCombineResult.Single<S>(blockEntity);
        }
        boolean isFirst = type == BlockType.FIRST;
        BlockPos neighbourPos = pos.relative(connectionResolver.apply(state));
        BlockState neighbourState = level.getBlockState(neighbourPos);
        if (neighbourState.is(state.getBlock())) {
            BlockType neighbourType = typeResolver.apply(neighbourState);
            if (neighbourType != BlockType.SINGLE && type != neighbourType
                && neighbourState.getValue(facingProperty) == state.getValue(facingProperty)) {
                if (blockedChecker.test(level, neighbourPos)) {
                    return Combiner::acceptNone;
                }
                S neighbour = entityType.getBlockEntity(level, neighbourPos);
                if (neighbour != null) {
                    return new NeighborCombineResult.Double<S>(isFirst ? blockEntity : neighbour, isFirst ? neighbour : blockEntity);
                }
            }
        }
        return new NeighborCombineResult.Single<S>(blockEntity);
    }
    public static enum BlockType {
        SINGLE, FIRST, SECOND;
    }
    public interface Combiner<S, T> {
        T acceptDouble(S first, S second);
        T acceptSingle(S single);
        T acceptNone();
    }
    public interface NeighborCombineResult<S> {
        <T> T apply(Combiner<? super S, T> callback);
        public static final class Double<S> implements NeighborCombineResult<S> {
            private final S first;
            private final S second;
            public Double(S first, S second) {
                this.first = first;
                this.second = second;
            }
            public <T> T apply(Combiner<? super S, T> callback) {
                return callback.acceptDouble(first, second);
            }
        }
        public static final class Single<S> implements NeighborCombineResult<S> {
            private final S single;
            public Single(S single) {
                this.single = single;
            }
            public <T> T apply(Combiner<? super S, T> callback) {
                return callback.acceptSingle(single);
            }
        }
    }
}
