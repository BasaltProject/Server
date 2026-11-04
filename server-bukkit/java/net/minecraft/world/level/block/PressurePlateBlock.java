// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BlockSetType;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
public class PressurePlateBlock extends BasePressurePlateBlock {
    protected PressurePlateBlock(int index) {
        super(index);
    }
    public static final BooleanProperty POWERED = BlockStateProperties.POWERED;
    protected PressurePlateBlock(BlockSetType type, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null, (net.minecraft.world.level.block.state.properties.BlockSetType) null);
    }
    protected int getSignalForState(BlockState state) {
        return state.getValue(POWERED) ? 15 : 0;
    }
    protected BlockState setSignalForState(BlockState state, int signal) {
        return state.setValue(POWERED, signal > 0);
    }
    protected int getSignalStrength(Level level, BlockPos pos) {
        if (!(level instanceof net.minecraft.server.level.ServerLevel server)) {
            return 0;
        }
        org.bukkit.util.BoundingBox area = new org.bukkit.util.BoundingBox(pos.getX() + 0.125, pos.getY(), pos.getZ() + 0.125,
            pos.getX() + 0.875, pos.getY() + 0.25, pos.getZ() + 0.875);
        return server.getWorld().getNearbyEntities(area).isEmpty() ? 0 : 15;
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
}
