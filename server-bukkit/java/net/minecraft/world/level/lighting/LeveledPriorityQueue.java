// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
public class LeveledPriorityQueue {
    private final int levelCount;
    private final it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet[] buckets;
    private int lowest;
    public LeveledPriorityQueue(int levelCount, int minSize) {
        this.levelCount = levelCount;
        this.buckets = new it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet[levelCount];
        for (int level = 0; level < levelCount; level++) {
            buckets[level] = new it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet(minSize, 0.5F);
        }
        this.lowest = levelCount;
    }
    private void rescan(int upperBound) {
        int from = lowest;
        lowest = upperBound;
        for (int level = from + 1; level < upperBound; level++) {
            if (!buckets[level].isEmpty()) {
                lowest = level;
                return;
            }
        }
    }
    public long removeFirstLong() {
        it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet head = buckets[lowest];
        long node = head.removeFirstLong();
        if (head.isEmpty()) {
            rescan(levelCount);
        }
        return node;
    }
    public boolean isEmpty() {
        return lowest >= levelCount;
    }
    public void dequeue(long node, int key, int upperBound) {
        it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet bucket = buckets[key];
        bucket.remove(node);
        if (bucket.isEmpty() && lowest == key) {
            rescan(upperBound);
        }
    }
    public void enqueue(long node, int key) {
        buckets[key].add(node);
        lowest = Math.min(lowest, key);
    }
}
