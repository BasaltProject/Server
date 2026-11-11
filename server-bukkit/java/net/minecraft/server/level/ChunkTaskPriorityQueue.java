// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import it.unimi.dsi.fastutil.longs.Long2ObjectLinkedOpenHashMap;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.world.level.ChunkPos;

public class ChunkTaskPriorityQueue {
    public static final int PRIORITY_LEVEL_COUNT = ChunkLevel.MAX_LEVEL + 2;
    private final List<Long2ObjectLinkedOpenHashMap<List<Runnable>>> buckets = new ArrayList<>(PRIORITY_LEVEL_COUNT);
    private volatile int top = PRIORITY_LEVEL_COUNT;
    private final String name;

    public ChunkTaskPriorityQueue(String name) {
        for (int priority = 0; priority < PRIORITY_LEVEL_COUNT; priority++) {
            buckets.add(new Long2ObjectLinkedOpenHashMap<>());
        }
        this.name = name;
    }

    private void skipEmptyBuckets() {
        while (hasWork() && buckets.get(top).isEmpty()) {
            top++;
        }
    }

    protected void resortChunkTasks(int oldPriority, ChunkPos pos, int newPriority) {
        if (oldPriority >= PRIORITY_LEVEL_COUNT) {
            return;
        }
        List<Runnable> moved = buckets.get(oldPriority).remove(pos.pack());
        if (oldPriority == top) {
            skipEmptyBuckets();
        }
        if (moved != null && !moved.isEmpty()) {
            buckets.get(newPriority).computeIfAbsent(pos.pack(), key -> new ArrayList<>()).addAll(moved);
            top = Math.min(top, newPriority);
        }
    }

    protected void submit(Runnable task, long chunkPos, int level) {
        buckets.get(level).computeIfAbsent(chunkPos, key -> new ArrayList<>()).add(task);
        top = Math.min(top, level);
    }

    protected void release(long pos, boolean unschedule) {
        for (Long2ObjectLinkedOpenHashMap<List<Runnable>> bucket : buckets) {
            List<Runnable> pending = bucket.get(pos);
            if (pending == null) {
                continue;
            }
            if (unschedule) {
                pending.clear();
            }
            if (pending.isEmpty()) {
                bucket.remove(pos);
            }
        }
        skipEmptyBuckets();
    }

    public TasksForChunk pop() {
        if (!hasWork()) {
            return null;
        }
        Long2ObjectLinkedOpenHashMap<List<Runnable>> bucket = buckets.get(top);
        long chunk = bucket.firstLongKey();
        List<Runnable> tasks = bucket.removeFirst();
        skipEmptyBuckets();
        return new TasksForChunk(chunk, tasks);
    }

    public boolean hasWork() {
        return top < PRIORITY_LEVEL_COUNT;
    }

    public String toString() {
        return name + " " + top + "...";
    }

    public static record TasksForChunk(long chunkPos, List<Runnable> tasks) {}
}
