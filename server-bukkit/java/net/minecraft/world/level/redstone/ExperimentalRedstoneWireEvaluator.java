// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;
import java.util.ArrayDeque;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.RedstoneWireBlock;
import net.minecraft.world.level.block.state.BlockState;
public class ExperimentalRedstoneWireEvaluator extends RedstoneWireEvaluator {
    public ExperimentalRedstoneWireEvaluator(RedstoneWireBlock wireBlock) {
        super(wireBlock);
    }
    public void updatePowerStrength(Level level, BlockPos initialPos, BlockState ignored, Orientation orientation, boolean shapeUpdateWiresAroundInitialPosition) {
        Map<BlockPos, Integer> strength = new java.util.LinkedHashMap<>();
        ArrayDeque<BlockPos> queue = new ArrayDeque<>();
        queue.add(initialPos.immutable());
        while (!queue.isEmpty()) {
            BlockPos at = queue.poll();
            BlockState here = level.getBlockState(at);
            if (strength.containsKey(at) || !here.is(wireBlock)) {
                continue;
            }
            int fromBlocks = getBlockSignal(level, at);
            int target = fromBlocks == 15 ? fromBlocks : Math.max(fromBlocks, getIncomingWireSignal(level, at));
            strength.put(at, target);
            if (target != here.getValue(RedstoneWireBlock.POWER)) {
                for (Direction side : Direction.Plane.HORIZONTAL) {
                    for (int rise = -1; rise <= 1; rise++) {
                        queue.add(at.relative(side).above(rise));
                    }
                }
            }
        }
        Set<BlockPos> touched = new LinkedHashSet<>();
        strength.forEach((at, target) -> {
            BlockState here = level.getBlockState(at);
            if (here.is(wireBlock) && here.getValue(RedstoneWireBlock.POWER) != target) {
                level.setBlock(at, here.setValue(RedstoneWireBlock.POWER, target), 2);
                touched.add(at);
                for (Direction side : Direction.values()) {
                    touched.add(at.relative(side));
                }
            }
        });
        touched.forEach(at -> level.updateNeighborsAt(at, wireBlock));
    }
    protected int getWireSignal(BlockPos pos, BlockState state) {
        return state.is(wireBlock) ? state.getValue(RedstoneWireBlock.POWER) : 0;

    }
}
