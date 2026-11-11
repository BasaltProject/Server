// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.lighting.DynamicGraphMinFixedPoint;

public abstract class ChunkTracker extends DynamicGraphMinFixedPoint {
    protected ChunkTracker(int levelCount, int minQueueSize, int minMapSize) {
        super(levelCount, minQueueSize, minMapSize);
    }

    protected boolean isSource(long node) {
        return node == ChunkPos.INVALID_CHUNK_POS;
    }

    protected void checkNeighborsAfterUpdate(long node, int level, boolean onlyDecrease) {
        if (onlyDecrease && level >= levelCount - 2) {
            return;
        }
        ChunkPos at = ChunkPos.unpack(node);
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                long next = ChunkPos.pack(at.x() + dx, at.z() + dz);
                if (next != node) {
                    checkNeighbor(node, next, level, onlyDecrease);
                }
            }
        }
    }

    protected int getComputedLevel(long node, long knownParent, int knownLevelFromParent) {
        int best = knownLevelFromParent;
        ChunkPos at = ChunkPos.unpack(node);
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                long next = ChunkPos.pack(at.x() + dx, at.z() + dz);
                if (next == node) {
                    next = ChunkPos.INVALID_CHUNK_POS;
                }
                if (next == knownParent) {
                    continue;
                }
                best = Math.min(best, computeLevelFromNeighbor(next, node, getLevel(next)));
                if (best == 0) {
                    return 0;
                }
            }
        }
        return best;
    }

    protected int computeLevelFromNeighbor(long from, long to, int fromLevel) {
        return from == ChunkPos.INVALID_CHUNK_POS ? getLevelFromSource(to) : fromLevel + 1;
    }

    protected abstract int getLevelFromSource(long to);

    public void update(long node, int newLevelFrom, boolean onlyDecreased) {
        checkEdge(ChunkPos.INVALID_CHUNK_POS, node, newLevelFrom, onlyDecreased);
    }
}
