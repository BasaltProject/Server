// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.Direction;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.EnumProperty;
public class RotatedPillarBlock extends Block {
    protected RotatedPillarBlock(int index) {
        super(index);
    }
    public static final EnumProperty<Direction.Axis> AXIS = BlockStateProperties.AXIS;
    public RotatedPillarBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    public static BlockState rotatePillar(BlockState state, Rotation rotation) {
        if (rotation != Rotation.CLOCKWISE_90 && rotation != Rotation.COUNTERCLOCKWISE_90) {
            return state;
        }
        return switch (state.getValue(AXIS)) {
            case X -> state.setValue(AXIS, Direction.Axis.Z);
            case Z -> state.setValue(AXIS, Direction.Axis.X);
            default -> state;
        };
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
}
