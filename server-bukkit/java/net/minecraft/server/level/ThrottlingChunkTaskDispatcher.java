// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.concurrent.Executor;
import java.util.stream.Collectors;
import net.minecraft.util.thread.TaskScheduler;
import net.minecraft.world.level.ChunkPos;

public class ThrottlingChunkTaskDispatcher extends ChunkTaskDispatcher {
    private final LongSet running = new LongOpenHashSet();
    private final int limit;
    private final String schedulerName;

    public ThrottlingChunkTaskDispatcher(TaskScheduler<Runnable> executor, Executor dispatcherExecutor, int maxChunksInExecution) {
        super(executor, dispatcherExecutor);
        this.limit = maxChunksInExecution;
        this.schedulerName = executor.name();
    }

    protected void onRelease(long key) {
        running.remove(key);
    }

    protected ChunkTaskPriorityQueue.TasksForChunk popTasks() {
        return running.size() < limit ? super.popTasks() : null;
    }

    protected void scheduleForExecution(ChunkTaskPriorityQueue.TasksForChunk tasksForChunk) {
        running.add(tasksForChunk.chunkPos());
        super.scheduleForExecution(tasksForChunk);
    }

    public String getDebugStatus() {
        String listed = running.longStream().mapToObj(key -> key + ":" + ChunkPos.unpack(key)).collect(Collectors.joining(","));
        return schedulerName + "=[" + listed + "], s=" + sleeping;
    }
}
