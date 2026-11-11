// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
public class KelpBlock extends GrowingPlantHeadBlock implements LiquidBlockContainer {
    protected KelpBlock(BlockBehaviour.Properties properties) {
        super(properties, Direction.UP, Block.column(16.0, 0.0, 9.0), true, 0.14);
    }
    protected boolean canGrowInto(BlockState state) {
        return state.is(Blocks.WATER);
    }
    protected Block getBodyBlock() {
        return Blocks.KELP_PLANT;
    }
    protected boolean canAttachTo(BlockState state) {
        return !state.is(net.minecraft.tags.BlockTags.CANNOT_SUPPORT_KELP);
    }
    public boolean canPlaceLiquid(LivingEntity user, BlockGetter level, BlockPos pos, BlockState state, Fluid type) {
        return false;
    }
    public boolean placeLiquid(LevelAccessor level, BlockPos pos, BlockState state, FluidState fluidState) {
        return false;
    }
    protected int getBlocksToGrowWhenBonemealed(RandomSource random) {
        return 1;
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
}
