// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.core.SectionPos;
import net.minecraft.world.level.lighting.DynamicGraphMinFixedPoint;

public abstract class SectionTracker extends DynamicGraphMinFixedPoint {
    protected SectionTracker(int levelCount, int minQueueSize, int minMapSize) {
        super(levelCount, minQueueSize, minMapSize);
    }

    protected void checkNeighborsAfterUpdate(long node, int level, boolean onlyDecrease) {
        if (onlyDecrease && level >= levelCount - 2) {
            return;
        }
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    long next = SectionPos.offset(node, dx, dy, dz);
                    if (next != node) {
                        checkNeighbor(node, next, level, onlyDecrease);
                    }
                }
            }
        }
    }

    protected int getComputedLevel(long node, long knownParent, int knownLevelFromParent) {
        int best = knownLevelFromParent;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    long next = SectionPos.offset(node, dx, dy, dz);
                    if (next == node) {
                        next = Long.MAX_VALUE;
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
        }
        return best;
    }

    protected int computeLevelFromNeighbor(long from, long to, int fromLevel) {
        return isSource(from) ? getLevelFromSource(to) : fromLevel + 1;
    }

    protected abstract int getLevelFromSource(long to);

    public void update(long node, int newLevelFrom, boolean onlyDecreased) {
        checkEdge(Long.MAX_VALUE, node, newLevelFrom, onlyDecreased);
    }
}
