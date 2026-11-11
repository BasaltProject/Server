// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
public class HugeMushroomBlock extends Block {
    public static final BooleanProperty NORTH = net.minecraft.world.level.block.state.properties.BlockStateProperties.NORTH;
    public static final BooleanProperty EAST = net.minecraft.world.level.block.state.properties.BlockStateProperties.EAST;
    public static final BooleanProperty SOUTH = net.minecraft.world.level.block.state.properties.BlockStateProperties.SOUTH;
    public static final BooleanProperty WEST = net.minecraft.world.level.block.state.properties.BlockStateProperties.WEST;
    public static final BooleanProperty UP = net.minecraft.world.level.block.state.properties.BlockStateProperties.UP;
    public static final BooleanProperty DOWN = net.minecraft.world.level.block.state.properties.BlockStateProperties.DOWN;
    public HugeMushroomBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return super.mirror(state, mirror);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
}
