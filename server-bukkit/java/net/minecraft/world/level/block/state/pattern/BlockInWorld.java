// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.pattern;

import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;

public class BlockInWorld {
    private final LevelReader level;
    private final BlockPos pos;
    private final boolean loadChunks;
    private BlockState state;
    private BlockEntity entity;
    private boolean entityCached;

    public BlockInWorld(LevelReader level, BlockPos pos, boolean loadChunks) {
        this.level = level;
        this.pos = pos.immutable();
        this.loadChunks = loadChunks;
    }

    public BlockState getState() {
        if (state == null && (loadChunks || level.hasChunkAt(pos))) {
            state = level.getBlockState(pos);
        }
        return state;
    }

    public BlockEntity getEntity() {
        if (!entityCached) {
            entity = level.getBlockEntity(pos);
            entityCached = true;
        }
        return entity;
    }

    public LevelReader getLevel() {
        return level;
    }

    public BlockPos getPos() {
        return pos;
    }

    public static Predicate<BlockInWorld> hasState(Predicate<BlockState> predicate) {
        return block -> block != null && predicate.test(block.getState());
    }
}
