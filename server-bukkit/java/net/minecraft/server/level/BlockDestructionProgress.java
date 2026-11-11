// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.core.BlockPos;

public class BlockDestructionProgress implements Comparable<BlockDestructionProgress> {
    private final int id;
    private final BlockPos pos;
    private int stage;
    private long lastTick;

    public BlockDestructionProgress(int id, BlockPos pos) {
        this.id = id;
        this.pos = pos;
    }

    public int getId() {
        return id;
    }

    public BlockPos getPos() {
        return pos;
    }

    public void setProgress(int progress) {
        stage = Math.min(progress, 10);
    }

    public int getProgress() {
        return stage;
    }

    public void updateTick(long tick) {
        lastTick = tick;
    }

    public long getUpdatedRenderTick() {
        return lastTick;
    }

    public boolean equals(Object o) {
        return o instanceof BlockDestructionProgress other && other.id == id;
    }

    public int hashCode() {
        return Integer.hashCode(id);
    }

    public int compareTo(BlockDestructionProgress o) {
        int byStage = Integer.compare(stage, o.stage);
        return byStage != 0 ? byStage : Integer.compare(id, o.id);
    }
}
