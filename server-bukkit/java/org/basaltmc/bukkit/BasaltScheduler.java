package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.Future;
import java.util.function.Consumer;
import org.bukkit.plugin.Plugin;
import org.bukkit.scheduler.BukkitRunnable;
import org.bukkit.scheduler.BukkitTask;
import org.bukkit.scheduler.BukkitWorker;
import org.basaltmc.bukkit.gen.BukkitSchedulerBase;

public final class BasaltScheduler extends BukkitSchedulerBase {
    private static final java.util.concurrent.ScheduledThreadPoolExecutor kWorkers = workers();

    private final Map<Long, Task> tasks = new ConcurrentHashMap<>();
    private long nextId = 1;

    private static java.util.concurrent.ScheduledThreadPoolExecutor workers() {
        java.util.concurrent.atomic.AtomicInteger counter = new java.util.concurrent.atomic.AtomicInteger();
        java.util.concurrent.ScheduledThreadPoolExecutor pool = new java.util.concurrent.ScheduledThreadPoolExecutor(
            4, runnable -> {
                Thread thread = new Thread(runnable, "Craft Scheduler Thread - " + counter.incrementAndGet());
                thread.setDaemon(true);
                return thread;
            });
        pool.setRemoveOnCancelPolicy(true);
        pool.setKeepAliveTime(30, java.util.concurrent.TimeUnit.SECONDS);
        pool.allowCoreThreadTimeOut(true);
        return pool;
    }

    private final class Task implements BukkitTask {
        final long id;
        final Plugin owner;
        final Consumer<? super BukkitTask> body;
        final boolean sync;
        final boolean repeating;
        volatile boolean cancelled;
        volatile boolean running;
        volatile java.util.concurrent.ScheduledFuture<?> worker;

        Task(long id, Plugin owner, Consumer<? super BukkitTask> body, boolean sync, boolean repeating) {
            this.id = id;
            this.owner = owner;
            this.body = body;
            this.sync = sync;
            this.repeating = repeating;
        }

        @Override
        public int getTaskId() {
            return (int) id;
        }

        @Override
        public Plugin getOwner() {
            return owner;
        }

        @Override
        public boolean isSync() {
            return sync;
        }

        @Override
        public boolean isCancelled() {
            return cancelled;
        }

        @Override
        public void cancel() {
            if (!cancelled) {
                cancelled = true;
                tasks.remove(id);
                if (worker != null) {
                    worker.cancel(false);
                } else if (sync) {
                    Natives.cancelTask(id);
                }
            }
        }
    }

    private BukkitTask schedule(Plugin plugin, Consumer<? super BukkitTask> body, long delay, long period, boolean sync) {
        long id;
        synchronized (this) {
            id = nextId++;
        }
        boolean repeating = period > 0;
        Task task = new Task(id, plugin, body, sync, repeating);
        tasks.put(id, task);
        int start = (int) Math.max(delay, 0);
        if (!sync) {
            long delayMs = start * 50L;
            task.worker = repeating
                ? kWorkers.scheduleAtFixedRate(() -> run(id), delayMs, period * 50L,
                    java.util.concurrent.TimeUnit.MILLISECONDS)
                : kWorkers.schedule(() -> run(id), delayMs, java.util.concurrent.TimeUnit.MILLISECONDS);
            return task;
        }
        // A task due now still waits a tick, as Bukkit's does when it is scheduled mid-tick.
        if (repeating) {
            Natives.scheduleEvery(id, start, (int) period);
        } else {
            Natives.scheduleAfter(id, start);
        }
        return task;
    }

    private static Consumer<BukkitTask> wrap(Runnable runnable) {
        return ignored -> runnable.run();
    }

    /** Called by the native task service when the tick of a task comes. */
    void run(long id) {
        Task task = tasks.get(id);
        if (task == null || task.cancelled) {
            return;
        }
        if (!task.repeating) {
            tasks.remove(id);
        }
        task.running = true;
        Blocks.begin();
        try {
            task.body.accept(task);
        } catch (Throwable t) {
            Natives.log(3, "Bukkit", "Task #" + id + " for " + task.owner.getName() + " generated an exception");
            t.printStackTrace();
        } finally {
            Blocks.end();
            task.running = false;
        }
    }

    @Override
    public int scheduleSyncDelayedTask(Plugin plugin, Runnable task, long delay) {
        return runTaskLater(plugin, task, delay).getTaskId();
    }

    @Override
    public int scheduleSyncDelayedTask(Plugin plugin, Runnable task) {
        return runTask(plugin, task).getTaskId();
    }

    @Override
    public int scheduleSyncDelayedTask(Plugin plugin, BukkitRunnable task, long delay) {
        return runTaskLater(plugin, (Runnable) task, delay).getTaskId();
    }

    @Override
    public int scheduleSyncDelayedTask(Plugin plugin, BukkitRunnable task) {
        return runTask(plugin, (Runnable) task).getTaskId();
    }

    @Override
    public int scheduleSyncRepeatingTask(Plugin plugin, Runnable task, long delay, long period) {
        return runTaskTimer(plugin, task, delay, period).getTaskId();
    }

    @Override
    public int scheduleSyncRepeatingTask(Plugin plugin, BukkitRunnable task, long delay, long period) {
        return runTaskTimer(plugin, (Runnable) task, delay, period).getTaskId();
    }

    @Override
    public int scheduleAsyncDelayedTask(Plugin plugin, Runnable task, long delay) {
        return runTaskLaterAsynchronously(plugin, task, delay).getTaskId();
    }

    @Override
    public int scheduleAsyncDelayedTask(Plugin plugin, Runnable task) {
        return runTaskAsynchronously(plugin, task).getTaskId();
    }

    @Override
    public int scheduleAsyncRepeatingTask(Plugin plugin, Runnable task, long delay, long period) {
        return runTaskTimerAsynchronously(plugin, task, delay, period).getTaskId();
    }

    @Override
    public <T> Future<T> callSyncMethod(Plugin plugin, Callable<T> task) {
        CompletableFuture<T> future = new CompletableFuture<>();
        runTask(plugin, () -> {
            try {
                future.complete(task.call());
            } catch (Throwable t) {
                future.completeExceptionally(t);
            }
        });
        return future;
    }

    @Override
    public void cancelTask(int id) {
        Task task = tasks.get((long) id);
        if (task != null) {
            task.cancel();
        }
    }

    @Override
    public void cancelTasks(Plugin plugin) {
        for (Task task : new ArrayList<>(tasks.values())) {
            if (task.owner == plugin) {
                task.cancel();
            }
        }
    }

    @Override
    public boolean isCurrentlyRunning(int id) {
        Task task = tasks.get((long) id);
        return task != null && task.running;
    }

    @Override
    public boolean isQueued(int id) {
        Task task = tasks.get((long) id);
        return task != null && !task.cancelled;
    }

    @Override
    public List<BukkitWorker> getActiveWorkers() {
        return List.of();
    }

    @Override
    public List<BukkitTask> getPendingTasks() {
        return new ArrayList<>(tasks.values());
    }

    @Override
    public Executor getMainThreadExecutor(Plugin plugin) {
        return command -> runTask(plugin, command);
    }

    @Override
    public BukkitTask runTask(Plugin plugin, Runnable task) {
        return schedule(plugin, wrap(task), 0, 0, true);
    }

    @Override
    public void runTask(Plugin plugin, Consumer<? super BukkitTask> task) {
        schedule(plugin, task, 0, 0, true);
    }

    @Override
    public BukkitTask runTask(Plugin plugin, BukkitRunnable task) {
        return runTask(plugin, (Runnable) task);
    }

    @Override
    public BukkitTask runTaskAsynchronously(Plugin plugin, Runnable task) {
        return schedule(plugin, wrap(task), 0, 0, false);
    }

    @Override
    public void runTaskAsynchronously(Plugin plugin, Consumer<? super BukkitTask> task) {
        schedule(plugin, task, 0, 0, false);
    }

    @Override
    public BukkitTask runTaskAsynchronously(Plugin plugin, BukkitRunnable task) {
        return runTaskAsynchronously(plugin, (Runnable) task);
    }

    @Override
    public BukkitTask runTaskLater(Plugin plugin, Runnable task, long delay) {
        return schedule(plugin, wrap(task), delay, 0, true);
    }

    @Override
    public void runTaskLater(Plugin plugin, Consumer<? super BukkitTask> task, long delay) {
        schedule(plugin, task, delay, 0, true);
    }

    @Override
    public BukkitTask runTaskLater(Plugin plugin, BukkitRunnable task, long delay) {
        return runTaskLater(plugin, (Runnable) task, delay);
    }

    @Override
    public BukkitTask runTaskLaterAsynchronously(Plugin plugin, Runnable task, long delay) {
        return schedule(plugin, wrap(task), delay, 0, false);
    }

    @Override
    public void runTaskLaterAsynchronously(Plugin plugin, Consumer<? super BukkitTask> task, long delay) {
        schedule(plugin, task, delay, 0, false);
    }

    @Override
    public BukkitTask runTaskLaterAsynchronously(Plugin plugin, BukkitRunnable task, long delay) {
        return runTaskLaterAsynchronously(plugin, (Runnable) task, delay);
    }

    @Override
    public BukkitTask runTaskTimer(Plugin plugin, Runnable task, long delay, long period) {
        return schedule(plugin, wrap(task), delay, Math.max(period, 1), true);
    }

    @Override
    public void runTaskTimer(Plugin plugin, Consumer<? super BukkitTask> task, long delay, long period) {
        schedule(plugin, task, delay, Math.max(period, 1), true);
    }

    @Override
    public BukkitTask runTaskTimer(Plugin plugin, BukkitRunnable task, long delay, long period) {
        return runTaskTimer(plugin, (Runnable) task, delay, period);
    }

    @Override
    public BukkitTask runTaskTimerAsynchronously(Plugin plugin, Runnable task, long delay, long period) {
        return schedule(plugin, wrap(task), delay, Math.max(period, 1), false);
    }

    @Override
    public void runTaskTimerAsynchronously(Plugin plugin, Consumer<? super BukkitTask> task, long delay, long period) {
        schedule(plugin, task, delay, Math.max(period, 1), false);
    }

    @Override
    public BukkitTask runTaskTimerAsynchronously(Plugin plugin, BukkitRunnable task, long delay, long period) {
        return runTaskTimerAsynchronously(plugin, (Runnable) task, delay, period);
    }
}
