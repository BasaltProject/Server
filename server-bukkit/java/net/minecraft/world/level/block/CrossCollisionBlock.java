// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import java.util.Map;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class CrossCollisionBlock extends Block implements SimpleWaterloggedBlock {
    protected CrossCollisionBlock(int index) {
        super(index);
    }
    public static final BooleanProperty NORTH = BlockStateProperties.NORTH;
    public static final BooleanProperty EAST = BlockStateProperties.EAST;
    public static final BooleanProperty SOUTH = BlockStateProperties.SOUTH;
    public static final BooleanProperty WEST = BlockStateProperties.WEST;
    public static final BooleanProperty WATERLOGGED = BlockStateProperties.WATERLOGGED;
    public static final Map<Direction, BooleanProperty> PROPERTY_BY_DIRECTION = Map.of(Direction.NORTH, NORTH, Direction.EAST, EAST, Direction.SOUTH, SOUTH, Direction.WEST, WEST);
    protected CrossCollisionBlock(float postWidth, float postHeight, float wallWidth, float wallHeight, float collisionHeight, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected Function<BlockState, VoxelShape> makeShapes(float postWidth, float postHeight, float wallWidth, float wallBottom, float wallTop) {
        VoxelShape post = Block.column(postWidth, 0.0, postHeight);
        Map<Direction, VoxelShape> arms = net.minecraft.world.phys.shapes.Shapes.rotateHorizontal(Block.boxZ(wallWidth, wallBottom, wallTop, 0.0, 8.0));
        return getShapeForEachState(state -> {
            VoxelShape shape = post;
            for (Map.Entry<Direction, BooleanProperty> entry : PROPERTY_BY_DIRECTION.entrySet()) {
                if (state.getValue(entry.getValue())) {
                    shape = net.minecraft.world.phys.shapes.Shapes.or(shape, arms.get(entry.getKey()));
                }
            }
            return shape;
        }, WATERLOGGED);
    }
    protected boolean propagatesSkylightDown(BlockState state) {
        return super.propagatesSkylightDown(state);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected VoxelShape getCollisionShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getCollisionShape(state, level, pos, context);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return super.mirror(state, mirror);
    }
}
