// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.IntConsumer;
import java.util.function.IntSupplier;
import net.minecraft.util.Unit;
import net.minecraft.util.thread.PriorityConsecutiveExecutor;
import net.minecraft.util.thread.StrictQueue;
import net.minecraft.util.thread.TaskScheduler;
import net.minecraft.world.level.ChunkPos;

public class ChunkTaskDispatcher implements ChunkHolder.LevelChangeListener, AutoCloseable {
    public static final int DISPATCHER_PRIORITY_COUNT = 4;
    private static final int PRIORITY_RESORT = 0;
    private static final int PRIORITY_RELEASE = 1;
    private static final int PRIORITY_SUBMIT = 2;
    private static final int PRIORITY_POLL = 3;
    private final ChunkTaskPriorityQueue queue;
    private final TaskScheduler<Runnable> executor;
    private final PriorityConsecutiveExecutor dispatcher;
    protected boolean sleeping = true;

    public ChunkTaskDispatcher(TaskScheduler<Runnable> executor, Executor dispatcherExecutor) {
        this.queue = new ChunkTaskPriorityQueue(executor.name() + "_queue");
        this.executor = executor;
        this.dispatcher = new PriorityConsecutiveExecutor(DISPATCHER_PRIORITY_COUNT, dispatcherExecutor, "dispatcher");
    }

    private void enqueue(int priority, Runnable work) {
        dispatcher.schedule(new StrictQueue.RunnableWithPriority(priority, work));
    }

    private void wakeIfSleeping() {
        if (sleeping) {
            sleeping = false;
            pollTask();
        }
    }

    public boolean hasWork() {
        return dispatcher.hasWork() || queue.hasWork();
    }

    public void onLevelChange(ChunkPos pos, IntSupplier oldLevel, int newLevel, IntConsumer setQueueLevel) {
        enqueue(PRIORITY_RESORT, () -> {
            queue.resortChunkTasks(oldLevel.getAsInt(), pos, newLevel);
            setQueueLevel.accept(newLevel);
        });
    }

    public void release(long pos, Runnable whenReleased, boolean clearQueue) {
        enqueue(PRIORITY_RELEASE, () -> {
            queue.release(pos, clearQueue);
            onRelease(pos);
            wakeIfSleeping();
            whenReleased.run();
        });
    }

    public void submit(Runnable task, long pos, IntSupplier level) {
        enqueue(PRIORITY_SUBMIT, () -> {
            queue.submit(task, pos, level.getAsInt());
            wakeIfSleeping();
        });
    }

    protected void pollTask() {
        enqueue(PRIORITY_POLL, () -> {
            ChunkTaskPriorityQueue.TasksForChunk next = popTasks();
            if (next == null) {
                sleeping = true;
            } else {
                scheduleForExecution(next);
            }
        });
    }

    protected void scheduleForExecution(ChunkTaskPriorityQueue.TasksForChunk tasksForChunk) {
        CompletableFuture<?>[] running = tasksForChunk.tasks().stream().map(task -> executor.<Unit>scheduleWithResult(done -> {
            task.run();
            done.complete(Unit.INSTANCE);
        })).toArray(CompletableFuture[]::new);
        CompletableFuture.allOf(running).thenAccept(ignored -> pollTask());
    }

    protected void onRelease(long key) {}

    protected ChunkTaskPriorityQueue.TasksForChunk popTasks() {
        return queue.pop();
    }

    public void close() {
        executor.close();
    }
}
