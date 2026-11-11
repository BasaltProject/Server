// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class BaseRailBlock extends Block implements SimpleWaterloggedBlock {
    private boolean isStraight;
    private static RailShape clockwise(RailShape shape) {
        return switch (shape) {
            case NORTH_SOUTH -> RailShape.EAST_WEST;
            case EAST_WEST -> RailShape.NORTH_SOUTH;
            case ASCENDING_EAST -> RailShape.ASCENDING_SOUTH;
            case ASCENDING_SOUTH -> RailShape.ASCENDING_WEST;
            case ASCENDING_WEST -> RailShape.ASCENDING_NORTH;
            case ASCENDING_NORTH -> RailShape.ASCENDING_EAST;
            case SOUTH_EAST -> RailShape.SOUTH_WEST;
            case SOUTH_WEST -> RailShape.NORTH_WEST;
            case NORTH_WEST -> RailShape.NORTH_EAST;
            case NORTH_EAST -> RailShape.SOUTH_EAST;
        };
    }
    public static final BooleanProperty WATERLOGGED = net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED;
    public static boolean isRail(Level level, BlockPos pos) {
        return isRail(level.getBlockState(pos));
    }
    public static boolean isRail(BlockState state) {
        return state.is(net.minecraft.tags.BlockTags.RAILS) && state.getBlock() instanceof BaseRailBlock;
    }
    protected BaseRailBlock(boolean isStraight, BlockBehaviour.Properties properties) {
        super((int) 0);
        this.isStraight = isStraight;
    }
    public boolean isStraight() {
        return isStraight;
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    protected BlockState updateState(BlockState state, Level level, BlockPos pos, boolean movedByPiston) {
        state = updateDir(level, pos, state, true);
        if (isStraight) {
            level.neighborChanged(state, pos, this, null, movedByPiston);
        }
        return state;
    }
    protected void neighborChanged(BlockState state, Level level, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) {
        super.neighborChanged(state, level, pos, block, orientation, movedByPiston);
    }
    protected void updateState(BlockState state, Level level, BlockPos pos, Block block) {
        return;
    }
    protected BlockState updateDir(Level level, BlockPos pos, BlockState state, boolean first) {
        if (level.isClientSide()) {
            return state;
        }
        RailShape current = state.getValue(getShapeProperty());
        return new RailState(level, pos, state).place(level.hasNeighborSignal(pos), first, current).getState();
    }
    protected void affectNeighborsAfterRemoval(BlockState state, ServerLevel level, BlockPos pos, boolean movedByPiston) {
        super.affectNeighborsAfterRemoval(state, level, pos, movedByPiston);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public abstract Property<RailShape> getShapeProperty();
    protected RailShape rotate(RailShape shape, Rotation rotation) {
        return switch (rotation) {
            case CLOCKWISE_90 -> clockwise(shape);
            case CLOCKWISE_180 -> clockwise(clockwise(shape));
            case COUNTERCLOCKWISE_90 -> clockwise(clockwise(clockwise(shape)));
            default -> shape;
        };
    }
    protected RailShape mirror(RailShape shape, Mirror mirror) {
        if (mirror == Mirror.LEFT_RIGHT) {
            return switch (shape) {
                case ASCENDING_NORTH -> RailShape.ASCENDING_SOUTH;
                case ASCENDING_SOUTH -> RailShape.ASCENDING_NORTH;
                case SOUTH_EAST -> RailShape.NORTH_EAST;
                case SOUTH_WEST -> RailShape.NORTH_WEST;
                case NORTH_WEST -> RailShape.SOUTH_WEST;
                case NORTH_EAST -> RailShape.SOUTH_EAST;
                default -> shape;
            };
        }
        if (mirror == Mirror.FRONT_BACK) {
            return switch (shape) {
                case ASCENDING_EAST -> RailShape.ASCENDING_WEST;
                case ASCENDING_WEST -> RailShape.ASCENDING_EAST;
                case SOUTH_EAST -> RailShape.SOUTH_WEST;
                case SOUTH_WEST -> RailShape.SOUTH_EAST;
                case NORTH_WEST -> RailShape.NORTH_EAST;
                case NORTH_EAST -> RailShape.NORTH_WEST;
                default -> shape;
            };
        }
        return shape;
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
}
