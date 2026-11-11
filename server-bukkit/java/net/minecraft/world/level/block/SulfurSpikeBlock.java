// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class SulfurSpikeBlock extends SpeleothemBlock {
    public SulfurSpikeBlock(BlockState blockToGrowOn, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockState) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected int getStalactiteLandingSound() {
        return 1052;
    }
    protected int getMaxGrowthLength() {
        return 2;
    }
}
