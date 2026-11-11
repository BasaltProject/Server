// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.EnumSet;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.material.Fluids;
import java.util.Collection;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class MultifaceBlock extends Block implements SimpleWaterloggedBlock {
    public static final BooleanProperty WATERLOGGED = net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED;
    protected static final Direction[] DIRECTIONS = Direction.values();
    public MultifaceBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public static Set<Direction> availableFaces(BlockState state) {
        Set<Direction> faces = EnumSet.noneOf(Direction.class);
        if (state.getBlock() instanceof MultifaceBlock) {
            for (Direction side : DIRECTIONS) {
                if (hasFace(state, side)) {
                    faces.add(side);
                }
            }
        }
        return faces;
    }
    public static Set<Direction> unpack(byte data) {
        Set<Direction> present = EnumSet.noneOf(Direction.class);
        for (Direction side : DIRECTIONS) {
            if ((data & (1 << side.ordinal())) != 0) {
                present.add(side);
            }
        }
        return present;
    }
    public static byte pack(Collection<Direction> directions) {
        int bits = 0;
        for (Direction side : directions) {
            bits |= 1 << side.ordinal();
        }
        return (byte) bits;
    }
    protected boolean isFaceSupported(Direction faceDirection) {
        return true;
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected boolean canBeReplaced(BlockState state, BlockPlaceContext context) {
        return super.canBeReplaced(state, context);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public boolean isValidStateForPlacement(BlockGetter level, BlockState oldState, BlockPos placementPos, Direction placementDirection) {
        if (!isFaceSupported(placementDirection) || oldState.is(this) && hasFace(oldState, placementDirection)) {
            return false;
        }
        BlockPos target = placementPos.relative(placementDirection);
        return canAttachTo(level, placementDirection, target, level.getBlockState(target));
    }
    public BlockState getStateForPlacement(BlockState oldState, BlockGetter level, BlockPos placementPos, Direction placementDirection) {
        if (!isValidStateForPlacement(level, oldState, placementPos, placementDirection)) {
            return null;
        }
        BlockState base = oldState.is(this) ? oldState : defaultBlockState();
        if (!oldState.is(this) && oldState.getFluidState().isSourceOfType(Fluids.WATER)) {
            base = base.setValue(BlockStateProperties.WATERLOGGED, true);
        }
        return base.setValue(getFaceProperty(placementDirection), true);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return super.mirror(state, mirror);
    }
    public static boolean hasFace(BlockState state, Direction faceDirection) {
        return state.getValueOrElse(getFaceProperty(faceDirection), false);
    }
    public static boolean canAttachTo(BlockGetter level, BlockPos pos, Direction directionTowardsNeighbour) {
        BlockPos target = pos.relative(directionTowardsNeighbour);
        return canAttachTo(level, directionTowardsNeighbour, target, level.getBlockState(target));
    }
    public static boolean canAttachTo(BlockGetter level, Direction directionTowardsNeighbour, BlockPos neighbourPos, BlockState neighbourState) {
        Direction towardsUs = directionTowardsNeighbour.getOpposite();
        return Block.isFaceFull(neighbourState.getBlockSupportShape(level, neighbourPos), towardsUs)
            || Block.isFaceFull(neighbourState.getCollisionShape(level, neighbourPos), towardsUs);
    }
    public static BooleanProperty getFaceProperty(Direction faceDirection) {
        return switch (faceDirection) {
            case NORTH -> BlockStateProperties.NORTH;
            case EAST -> BlockStateProperties.EAST;
            case SOUTH -> BlockStateProperties.SOUTH;
            case WEST -> BlockStateProperties.WEST;
            case UP -> BlockStateProperties.UP;
            case DOWN -> BlockStateProperties.DOWN;
        };
    }
    protected static boolean hasAnyFace(BlockState state) {
        for (Direction side : DIRECTIONS) {
            if (hasFace(state, side)) {
                return true;
            }
        }
        return false;


    }
}
