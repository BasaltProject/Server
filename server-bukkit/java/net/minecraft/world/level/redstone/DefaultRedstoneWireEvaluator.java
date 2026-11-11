// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;
import java.util.LinkedHashSet;
import java.util.Set;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.RedstoneWireBlock;
import net.minecraft.world.level.block.state.BlockState;
public class DefaultRedstoneWireEvaluator extends RedstoneWireEvaluator {
    public DefaultRedstoneWireEvaluator(RedstoneWireBlock wireBlock) {
        super(wireBlock);
    }
    public void updatePowerStrength(Level level, BlockPos pos, BlockState state, Orientation orientation, boolean skipShapeUpdates) {
        int fromBlocks = getBlockSignal(level, pos);
        int target = fromBlocks == 15 ? fromBlocks : Math.max(fromBlocks, getIncomingWireSignal(level, pos));
        if (state.getValue(RedstoneWireBlock.POWER) == target) {
            return;
        }
        if (level.getBlockState(pos) == state) {
            level.setBlock(pos, state.setValue(RedstoneWireBlock.POWER, target), 2);
        }
        Set<BlockPos> touched = new LinkedHashSet<>();
        touched.add(pos);
        for (Direction side : Direction.values()) {
            touched.add(pos.relative(side));
        }
        touched.forEach(at -> level.updateNeighborsAt(at, wireBlock));
    }
}
