// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.util.Mth;
import java.util.function.LongPredicate;
public abstract class DynamicGraphMinFixedPoint {
    public static final long SOURCE = Long.MAX_VALUE;
    private static final int kUnset = 255;
    private final LeveledPriorityQueue pending;
    private final it.unimi.dsi.fastutil.longs.Long2ByteMap computed;
    private volatile boolean dirty;
    protected final int levelCount;
    protected DynamicGraphMinFixedPoint(int levelCount, int minQueueSize, int minMapSize) {
        if (levelCount >= 254) {
            throw new IllegalArgumentException("Level count must be < 254.");
        }
        this.levelCount = levelCount;
        this.pending = new LeveledPriorityQueue(levelCount, minQueueSize);
        this.computed = new it.unimi.dsi.fastutil.longs.Long2ByteOpenHashMap(minMapSize, 0.5F);
        this.computed.defaultReturnValue((byte) -1);
    }
    private int priorityOf(int level, int target) {
        return Math.min(Math.min(level, target), levelCount - 1);
    }
    private int clampLevel(int level) {
        return Mth.clamp(level, 0, levelCount - 1);
    }
    private void relax(long from, long to, int fromLevel, int toLevel, int stored, boolean onlyDecreased) {
        if (isSource(to)) {
            return;
        }
        fromLevel = clampLevel(fromLevel);
        toLevel = clampLevel(toLevel);
        boolean fresh = stored == kUnset;
        if (fresh) {
            stored = toLevel;
        }
        int next = onlyDecreased ? Math.min(stored, fromLevel) : clampLevel(getComputedLevel(to, from, fromLevel));
        int oldPriority = priorityOf(toLevel, stored);
        if (toLevel != next) {
            int newPriority = priorityOf(toLevel, next);
            if (oldPriority != newPriority && !fresh) {
                pending.dequeue(to, oldPriority, newPriority);
            }
            pending.enqueue(to, newPriority);
            computed.put(to, (byte) next);
        } else if (!fresh) {
            pending.dequeue(to, oldPriority, levelCount);
            computed.remove(to);
        }
    }
    protected void removeFromQueue(long node) {
        int stored = computed.remove(node) & kUnset;
        if (stored != kUnset) {
            pending.dequeue(node, priorityOf(getLevel(node), stored), levelCount);
            dirty = !pending.isEmpty();
        }
    }
    public void removeIf(LongPredicate pred) {
        it.unimi.dsi.fastutil.longs.LongList doomed = new it.unimi.dsi.fastutil.longs.LongArrayList();
        for (long node : computed.keySet()) {
            if (pred.test(node)) {
                doomed.add(node);
            }
        }
        for (long node : doomed) {
            removeFromQueue(node);
        }
    }
    protected void checkNode(long node) {
        checkEdge(node, node, levelCount - 1, false);
    }
    protected void checkEdge(long from, long to, int newLevelFrom, boolean onlyDecreased) {
        relax(from, to, newLevelFrom, getLevel(to), computed.get(to) & kUnset, onlyDecreased);
        dirty = !pending.isEmpty();
    }
    protected final void checkNeighbor(long from, long to, int level, boolean onlyDecreased) {
        int stored = computed.get(to) & kUnset;
        int fromLevel = clampLevel(computeLevelFromNeighbor(from, to, level));
        if (onlyDecreased) {
            relax(from, to, fromLevel, getLevel(to), stored, true);
            return;
        }
        boolean fresh = stored == kUnset;
        int baseline = fresh ? clampLevel(getLevel(to)) : stored;
        if (fromLevel == baseline) {
            relax(from, to, levelCount - 1, fresh ? baseline : getLevel(to), stored, false);
        }
    }
    protected final boolean hasWork() {
        return dirty;
    }
    protected final int runUpdates(int count) {
        while (count > 0 && !pending.isEmpty()) {
            count--;
            long node = pending.removeFirstLong();
            int level = clampLevel(getLevel(node));
            int target = computed.remove(node) & kUnset;
            if (target < level) {
                setLevel(node, target);
                checkNeighborsAfterUpdate(node, target, true);
            } else if (target > level) {
                setLevel(node, levelCount - 1);
                if (target != levelCount - 1) {
                    pending.enqueue(node, priorityOf(levelCount - 1, target));
                    computed.put(node, (byte) target);
                }
                checkNeighborsAfterUpdate(node, level, false);
            }
        }
        dirty = !pending.isEmpty();
        return count;
    }
    public int getQueueSize() {
        return computed.size();
    }
    protected boolean isSource(long node) {
        return node == Long.MAX_VALUE;
    }
    protected abstract int getComputedLevel(long node, long knownParent, int knownLevelFromParent);
    protected abstract void checkNeighborsAfterUpdate(long node, int level, boolean onlyDecrease);
    protected abstract int getLevel(long node);
    protected abstract void setLevel(long node, int level);
    protected abstract int computeLevelFromNeighbor(long from, long to, int fromLevel);
}
