// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.pathfinder.PathComputationType;
public class ChorusPlantBlock extends PipeBlock {
    protected ChorusPlantBlock(BlockBehaviour.Properties properties) {
        super((float) 0, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public static BlockState getStateWithConnections(BlockGetter level, BlockPos pos, BlockState defaultState) {
        Block self = defaultState.getBlock();
        java.util.function.Predicate<BlockState> linked = other -> other.is(self) || other.is(Blocks.CHORUS_FLOWER);
        BlockState linkedDown = level.getBlockState(pos.below());
        return defaultState
            .trySetValue(BlockStateProperties.DOWN, linked.test(linkedDown) || linkedDown.is(net.minecraft.tags.BlockTags.SUPPORTS_CHORUS_PLANT))
            .trySetValue(BlockStateProperties.UP, linked.test(level.getBlockState(pos.above())))
            .trySetValue(BlockStateProperties.NORTH, linked.test(level.getBlockState(pos.north())))
            .trySetValue(BlockStateProperties.EAST, linked.test(level.getBlockState(pos.east())))
            .trySetValue(BlockStateProperties.SOUTH, linked.test(level.getBlockState(pos.south())))
            .trySetValue(BlockStateProperties.WEST, linked.test(level.getBlockState(pos.west())));
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
}
