// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.redstone.Orientation;
public class SpongeBlock extends Block {
    private static BlockPos.TraversalNodeStatus soakCell(Level level, BlockPos origin, BlockPos cell) {
        if (cell.equals(origin)) {
            return BlockPos.TraversalNodeStatus.ACCEPT;
        }
        BlockState st = level.getBlockState(cell);
        if (!level.getFluidState(cell).is(net.minecraft.tags.FluidTags.WATER)) {
            return BlockPos.TraversalNodeStatus.SKIP;
        }
        Block b = st.getBlock();
        if (b instanceof BucketPickup bp && !bp.pickupBlock(null, level, cell, st).isEmpty()) {
            return BlockPos.TraversalNodeStatus.ACCEPT;
        }
        boolean plant = st.is(Blocks.KELP) || st.is(Blocks.KELP_PLANT) || st.is(Blocks.SEAGRASS) || st.is(Blocks.TALL_SEAGRASS);
        if (!(b instanceof LiquidBlock) && !plant) {
            return BlockPos.TraversalNodeStatus.SKIP;
        }
        if (plant) {
            dropResources(st, level, cell, st.hasBlockEntity() ? level.getBlockEntity(cell) : null);
        }
        level.setBlockAndUpdate(cell, Blocks.AIR.defaultBlockState());
        return BlockPos.TraversalNodeStatus.ACCEPT;
    }


    public static final int MAX_DEPTH = 6;
    public static final int MAX_COUNT = 64;
    protected SpongeBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    protected void neighborChanged(BlockState state, Level level, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) {
        super.neighborChanged(state, level, pos, block, orientation, movedByPiston);
    }
    protected void tryAbsorbWater(Level level, BlockPos pos) {
        int visited = BlockPos.breadthFirstTraversal(pos, 6, 65,
            (from, out) -> {
                for (Direction d : Direction.values()) {
                    out.accept(from.relative(d));
                }
            },
            cell -> soakCell(level, pos, cell));
        if (visited > 1) {
            level.setBlock(pos, Blocks.WET_SPONGE.defaultBlockState(), 2);
            level.playSound(null, pos, SoundEvents.SPONGE_ABSORB, SoundSource.BLOCKS, 1.0F, 1.0F);
        }
    }
}
