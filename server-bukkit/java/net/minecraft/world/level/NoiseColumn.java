// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.BlockColumn;

public final class NoiseColumn implements BlockColumn {
    private final int minY;
    private final BlockState[] column;

    public NoiseColumn(int minY, BlockState[] column) {
        this.minY = minY;
        this.column = column;
    }

    public BlockState getBlock(int blockY) {
        int index = blockY - minY;
        return index >= 0 && index < column.length ? column[index] : Blocks.AIR.defaultBlockState();
    }

    public void setBlock(int blockY, BlockState state) {
        int index = blockY - minY;
        if (index < 0 || index >= column.length) {
            throw new IllegalArgumentException("Outside of column height: " + blockY);
        }
        column[index] = state;
    }
}
