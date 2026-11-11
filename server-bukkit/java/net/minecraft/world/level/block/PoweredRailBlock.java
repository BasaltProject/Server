// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.block.state.properties.RailShape;
public class PoweredRailBlock extends BaseRailBlock {
    public static final EnumProperty<RailShape> SHAPE = net.minecraft.world.level.block.state.properties.BlockStateProperties.RAIL_SHAPE_STRAIGHT;
    public static final BooleanProperty POWERED = net.minecraft.world.level.block.state.properties.BlockStateProperties.POWERED;
    protected PoweredRailBlock(BlockBehaviour.Properties properties) {
        super(false, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected boolean findPoweredRailSignal(Level level, BlockPos pos, BlockState state, boolean forward, int searchDepth) {
        if (searchDepth >= 8) {
            return false;
        }
        RailShape shape = state.getValue(SHAPE);
        int dx = 0;
        int dy = 0;
        int dz = 0;
        boolean lookBelow = true;
        RailShape axis = shape;
        switch (shape) {
            case NORTH_SOUTH -> dz = forward ? 1 : -1;
            case EAST_WEST -> dx = forward ? -1 : 1;
            case ASCENDING_EAST -> {
                dx = forward ? -1 : 1;
                dy = forward ? 0 : 1;
                axis = RailShape.EAST_WEST;
            }
            case ASCENDING_WEST -> {
                dx = forward ? -1 : 1;
                dy = forward ? 1 : 0;
                axis = RailShape.EAST_WEST;
            }
            case ASCENDING_NORTH -> {
                dz = forward ? 1 : -1;
                dy = forward ? 0 : 1;
                axis = RailShape.NORTH_SOUTH;
            }
            case ASCENDING_SOUTH -> {
                dz = forward ? 1 : -1;
                dy = forward ? 1 : 0;
                axis = RailShape.NORTH_SOUTH;
            }
            default -> { }
        }
        lookBelow = dy == 0;
        BlockPos next = pos.offset(dx, dy, dz);
        return isSameRailWithPower(level, next, forward, searchDepth, axis)
            || (lookBelow && isSameRailWithPower(level, next.below(), forward, searchDepth, axis));
    }
    protected boolean isSameRailWithPower(Level level, BlockPos pos, boolean forward, int searchDepth, RailShape dir) {
        BlockState other = level.getBlockState(pos);
        if (!other.is(this)) {
            return false;
        }
        RailShape otherShape = other.getValue(SHAPE);
        boolean crossesEW = dir == RailShape.EAST_WEST && (otherShape == RailShape.NORTH_SOUTH
            || otherShape == RailShape.ASCENDING_NORTH || otherShape == RailShape.ASCENDING_SOUTH);
        boolean crossesNS = dir == RailShape.NORTH_SOUTH && (otherShape == RailShape.EAST_WEST
            || otherShape == RailShape.ASCENDING_EAST || otherShape == RailShape.ASCENDING_WEST);
        if (crossesEW || crossesNS || !other.getValue(POWERED)) {
            return false;
        }
        return level.hasNeighborSignal(pos) || findPoweredRailSignal(level, pos, other, forward, searchDepth + 1);
    }
    protected void updateState(BlockState state, Level level, BlockPos pos, Block block) {
        boolean wasPowered = state.getValue(POWERED);
        boolean powered = level.hasNeighborSignal(pos)
            || findPoweredRailSignal(level, pos, state, true, 0)
            || findPoweredRailSignal(level, pos, state, false, 0);
        if (powered == wasPowered) {
            return;
        }
        level.setBlockAndUpdate(pos, state.setValue(POWERED, powered));
        level.updateNeighborsAt(pos.below(), this);
        if (state.getValue(SHAPE).isSlope()) {
            level.updateNeighborsAt(pos.above(), this);
        }
    }
    public Property<RailShape> getShapeProperty() {
        return SHAPE;
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
