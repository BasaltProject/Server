// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.block.state.BlockState;
public class UpgradeData {
    protected UpgradeData() {
    }
    public static final UpgradeData EMPTY = new UpgradeData();
    public UpgradeData(CompoundTag tag, LevelHeightAccessor levelHeightAccessor) {
    }
    public void upgrade(LevelChunk chunk) {
        return;
    }
    public boolean isEmpty() {
        return true;
    }
    public CompoundTag write() {
        return new CompoundTag();
    }
    public UpgradeData copy() {
        return this;
    }
    public interface BlockFixer {
        BlockState updateShape(BlockState state, Direction direction, BlockState neighbour, LevelAccessor level, BlockPos pos, BlockPos neighbourPos);
        default void processChunk(LevelAccessor level) {
            return;
        }
    }
    static enum BlockFixers implements BlockFixer {
        BLACKLIST, DEFAULT, CHEST, LEAVES, STEM_BLOCK;
        public static final Direction[] DIRECTIONS = Direction.values();
        public BlockState updateShape(BlockState state, Direction direction, BlockState neighbour, LevelAccessor level, BlockPos pos, BlockPos neighbourPos) {
            return state;
        }
        public void processChunk(LevelAccessor level) {
            return;
        }
    }
}
