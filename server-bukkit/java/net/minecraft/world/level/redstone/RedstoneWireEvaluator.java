// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.RedstoneWireBlock;
import net.minecraft.world.level.block.state.BlockState;
public abstract class RedstoneWireEvaluator {
    protected final RedstoneWireBlock wireBlock;
    protected RedstoneWireEvaluator(RedstoneWireBlock wireBlock) {
        this.wireBlock = wireBlock;
    }
    public abstract void updatePowerStrength(Level level, BlockPos pos, BlockState state, Orientation orientation, boolean skipShapeUpdates);
    protected int getBlockSignal(Level level, BlockPos pos) {
        return wireBlock.getBlockSignal(level, pos);
    }
    protected int getWireSignal(BlockPos pos, BlockState state) {
        return state.is(wireBlock) ? state.getValue(RedstoneWireBlock.POWER) : 0;
    }
    protected int getIncomingWireSignal(Level level, BlockPos pos) {
        int strongest = 0;
        BlockPos above = pos.above();
        for (Direction side : Direction.Plane.HORIZONTAL) {
            BlockPos beside = pos.relative(side);
            BlockState besideState = level.getBlockState(beside);
            strongest = Math.max(strongest, getWireSignal(beside, besideState));
            boolean conducts = besideState.isRedstoneConductor(level, beside);
            if (conducts && !level.getBlockState(above).isRedstoneConductor(level, above)) {
                BlockPos climb = beside.above();
                strongest = Math.max(strongest, getWireSignal(climb, level.getBlockState(climb)));
            } else if (!conducts) {
                BlockPos drop = beside.below();
                strongest = Math.max(strongest, getWireSignal(drop, level.getBlockState(drop)));
            }
        }
        return Math.max(0, strongest - 1);
    }
}
