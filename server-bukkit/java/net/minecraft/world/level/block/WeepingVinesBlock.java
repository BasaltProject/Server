// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class WeepingVinesBlock extends GrowingPlantHeadBlock {
    public WeepingVinesBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null, (net.minecraft.core.Direction) null, (net.minecraft.world.phys.shapes.VoxelShape) null, false, (double) 0);
    }
    protected int getBlocksToGrowWhenBonemealed(RandomSource random) {
        return NetherVines.getBlocksToGrowWhenBonemealed(random);
    }
    protected Block getBodyBlock() {
        return Blocks.WEEPING_VINES_PLANT;
    }
    protected boolean canGrowInto(BlockState state) {
        return NetherVines.isValidGrowthState(state);
    }
}
