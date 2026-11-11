// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
public class CauldronBlock extends AbstractCauldronBlock {
    public CauldronBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null, (net.minecraft.core.cauldron.CauldronInteraction.Dispatcher) null);
    }
    public boolean isFull(BlockState state) {
        return false;
    }
    protected static boolean shouldHandlePrecipitation(Level level, Biome.Precipitation precipitation) {
        if (precipitation == Biome.Precipitation.RAIN) {
            return level.getRandom().nextFloat() < 0.05F;
        }
        if (precipitation == Biome.Precipitation.SNOW) {
            return level.getRandom().nextFloat() < 0.1F;
        }
        return false;
    }
    public void handlePrecipitation(BlockState state, Level level, BlockPos pos, Biome.Precipitation precipitation) {
        super.handlePrecipitation(state, level, pos, precipitation);
    }
    protected boolean canReceiveStalactiteDrip(Fluid fluid) {
        return true;
    }
    protected void receiveStalactiteDrip(BlockState state, Level level, BlockPos pos, Fluid fluid) {
        BlockState newState;
        int event;
        if (fluid == net.minecraft.world.level.material.Fluids.WATER) {
            newState = Blocks.WATER_CAULDRON.defaultBlockState();
            event = 1047;
        } else if (fluid == net.minecraft.world.level.material.Fluids.LAVA) {
            newState = Blocks.LAVA_CAULDRON.defaultBlockState();
            event = 1046;
        } else {
            return;
        }
        level.setBlockAndUpdate(pos, newState);
        level.gameEvent(net.minecraft.world.level.gameevent.GameEvent.BLOCK_CHANGE, pos, net.minecraft.world.level.gameevent.GameEvent.Context.of(newState));
        level.levelEvent(event, pos, 0);
    }
}
