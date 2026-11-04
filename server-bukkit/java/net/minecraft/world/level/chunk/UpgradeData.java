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
    public static final UpgradeData EMPTY = null;
    private UpgradeData(LevelHeightAccessor levelHeightAccessor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#<init>");
    }
    public UpgradeData(CompoundTag tag, LevelHeightAccessor levelHeightAccessor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#<init>");
    }
    private UpgradeData(UpgradeData source) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#<init>");
    }
    public void upgrade(LevelChunk chunk) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#upgrade");
    }
    public boolean isEmpty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#isEmpty");
    }
    public CompoundTag write() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#write");
    }
    public UpgradeData copy() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData#copy");
    }
    public interface BlockFixer {
        BlockState updateShape(BlockState state, Direction direction, BlockState neighbour, LevelAccessor level, BlockPos pos, BlockPos neighbourPos);
        default void processChunk(LevelAccessor level) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData.BlockFixer#processChunk");
        }
    }
    static enum BlockFixers implements BlockFixer {
        BLACKLIST, DEFAULT, CHEST, LEAVES, STEM_BLOCK;
        public static final Direction[] DIRECTIONS = null;
        public BlockState updateShape(BlockState state, Direction direction, BlockState neighbour, LevelAccessor level, BlockPos pos, BlockPos neighbourPos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData.BlockFixers#updateShape");
        }
        public void processChunk(LevelAccessor level) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.UpgradeData.BlockFixers#processChunk");
        }
    }
}
