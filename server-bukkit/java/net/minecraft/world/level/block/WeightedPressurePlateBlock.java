// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BlockSetType;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
public class WeightedPressurePlateBlock extends BasePressurePlateBlock {
    protected WeightedPressurePlateBlock(int index) {
        super(index);
    }
    public static final IntegerProperty POWER = BlockStateProperties.POWER;
    protected WeightedPressurePlateBlock(int maxWeight, BlockSetType type, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null, (net.minecraft.world.level.block.state.properties.BlockSetType) null);
    }
    protected int getSignalStrength(Level level, BlockPos pos) {
        if (!(level instanceof net.minecraft.server.level.ServerLevel server)) {
            return 0;
        }
        int maxWeight = blockKey().getPath().equals("heavy_weighted_pressure_plate") ? 150 : 15;
        org.bukkit.util.BoundingBox area = new org.bukkit.util.BoundingBox(pos.getX() + 0.125, pos.getY(), pos.getZ() + 0.125,
            pos.getX() + 0.875, pos.getY() + 0.25, pos.getZ() + 0.875);
        int count = Math.min(server.getWorld().getNearbyEntities(area).size(), maxWeight);
        return count > 0 ? (int) Math.ceil(Math.min(maxWeight, count) / (float) maxWeight * 15.0F) : 0;
    }
    protected int getSignalForState(BlockState state) {
        return state.getValue(POWER);
    }
    protected BlockState setSignalForState(BlockState state, int signal) {
        return state.setValue(POWER, signal);
    }
    protected int getPressedTime() {
        return super.getPressedTime();
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
}
