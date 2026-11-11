// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.block.state.properties.RailShape;
public class RailBlock extends BaseRailBlock {
    public static final EnumProperty<RailShape> SHAPE = net.minecraft.world.level.block.state.properties.BlockStateProperties.RAIL_SHAPE;
    protected RailBlock(BlockBehaviour.Properties properties) {
        super(false, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected void updateState(BlockState state, Level level, BlockPos pos, Block block) {
        if (block.defaultBlockState().isSignalSource() && new RailState(level, pos, state).countPotentialConnections() == 3) {
            updateDir(level, pos, state, false);
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
