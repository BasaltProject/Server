package org.basaltmc.bukkit;

import io.papermc.paper.threadedregions.scheduler.AsyncScheduler;
import io.papermc.paper.threadedregions.scheduler.EntityScheduler;
import io.papermc.paper.threadedregions.scheduler.GlobalRegionScheduler;
import io.papermc.paper.threadedregions.scheduler.RegionScheduler;
import io.papermc.paper.threadedregions.scheduler.ScheduledTask;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.entity.Entity;
import org.bukkit.plugin.Plugin;
import org.bukkit.scheduler.BukkitScheduler;
import org.bukkit.scheduler.BukkitTask;

/** The region schedulers over the one tick Basalt runs a world on: a region is the whole server. */
public final class PaperSchedulers {
    private PaperSchedulers() {}

    private static BukkitScheduler scheduler() {
        return Bukkit.getScheduler();
    }

    private static final class Task implements ScheduledTask {
        private final Plugin plugin;
        private final boolean repeating;
        private volatile BukkitTask task;
        private volatile boolean cancelled;
        private volatile boolean running;
        private volatile boolean finished;

        Task(Plugin plugin, boolean repeating) {
            this.plugin = plugin;
            this.repeating = repeating;
        }

        void bind(BukkitTask handle) {
            task = handle;
            if (cancelled) {
                handle.cancel();
            }
        }

        void around(Runnable body) {
            running = true;
            try {
                body.run();
            } finally {
                running = false;
                finished = !repeating;
            }
        }

        @Override
        public Plugin getOwningPlugin() {
            return plugin;
        }

        @Override
        public boolean isRepeatingTask() {
            return repeating;
        }

        @Override
        public CancelledState cancel() {
            if (cancelled) {
                return running && repeating ? CancelledState.NEXT_RUNS_CANCELLED_ALREADY
                    : CancelledState.CANCELLED_ALREADY;
            }
            if (finished) {
                return CancelledState.ALREADY_EXECUTED;
            }
            cancelled = true;
            BukkitTask handle = task;
            if (handle != null) {
                handle.cancel();
            }
            return running ? CancelledState.NEXT_RUNS_CANCELLED : CancelledState.CANCELLED_BY_CALLER;
        }

        @Override
        public ExecutionState getExecutionState() {
            if (cancelled) {
                return running ? ExecutionState.CANCELLED_RUNNING : ExecutionState.CANCELLED;
            }
            if (running) {
                return ExecutionState.RUNNING;
            }
            return finished ? ExecutionState.FINISHED : ExecutionState.IDLE;
        }
    }

    private static void positive(long ticks, String what) {
        if (ticks <= 0) {
            throw new IllegalArgumentException(what + " ticks may not be <= 0");
        }
    }

    private static ScheduledTask sync(Plugin plugin, Consumer<ScheduledTask> body, long delay, long period,
        Runnable retired) {
        Task task = new Task(plugin, period > 0);
        Runnable run = () -> task.around(() -> body.accept(task));
        task.bind(period > 0 ? scheduler().runTaskTimer(plugin, run, delay, period)
            : delay > 0 ? scheduler().runTaskLater(plugin, run, delay) : scheduler().runTask(plugin, run));
        return task;
    }

    private static ScheduledTask async(Plugin plugin, Consumer<ScheduledTask> body, long delayMs, long periodMs) {
        Task task = new Task(plugin, periodMs > 0);
        Runnable run = () -> task.around(() -> body.accept(task));
        long delay = Math.max(0L, delayMs) / 50L;
        task.bind(periodMs > 0 ? scheduler().runTaskTimerAsynchronously(plugin, run, delay, Math.max(1L, periodMs / 50L))
            : delayMs > 0 ? scheduler().runTaskLaterAsynchronously(plugin, run, delay)
            : scheduler().runTaskAsynchronously(plugin, run));
        return task;
    }

    public static final class Global implements GlobalRegionScheduler {
        @Override
        public void execute(Plugin plugin, Runnable run) {
            scheduler().runTask(plugin, run);
        }

        @Override
        public ScheduledTask run(Plugin plugin, Consumer<ScheduledTask> task) {
            return sync(plugin, task, 0, 0, null);
        }

        @Override
        public ScheduledTask runDelayed(Plugin plugin, Consumer<ScheduledTask> task, long delay) {
            positive(delay, "Delay");
            return sync(plugin, task, delay, 0, null);
        }

        @Override
        public ScheduledTask runAtFixedRate(Plugin plugin, Consumer<ScheduledTask> task, long initialDelay,
            long period) {
            positive(initialDelay, "Initial delay");
            positive(period, "Period");
            return sync(plugin, task, initialDelay, period, null);
        }

        @Override
        public void cancelTasks(Plugin plugin) {
            scheduler().cancelTasks(plugin);
        }
    }

    public static final class Region implements RegionScheduler {
        @Override
        public void execute(Plugin plugin, World world, int chunkX, int chunkZ, Runnable run) {
            scheduler().runTask(plugin, run);
        }

        @Override
        public ScheduledTask run(Plugin plugin, World world, int chunkX, int chunkZ, Consumer<ScheduledTask> task) {
            return sync(plugin, task, 0, 0, null);
        }

        @Override
        public ScheduledTask runDelayed(Plugin plugin, World world, int chunkX, int chunkZ,
            Consumer<ScheduledTask> task, long delay) {
            positive(delay, "Delay");
            return sync(plugin, task, delay, 0, null);
        }

        @Override
        public ScheduledTask runAtFixedRate(Plugin plugin, World world, int chunkX, int chunkZ,
            Consumer<ScheduledTask> task, long initialDelay, long period) {
            positive(initialDelay, "Initial delay");
            positive(period, "Period");
            return sync(plugin, task, initialDelay, period, null);
        }
    }

    public static final class Async implements AsyncScheduler {
        @Override
        public ScheduledTask runNow(Plugin plugin, Consumer<ScheduledTask> task) {
            return async(plugin, task, 0, 0);
        }

        @Override
        public ScheduledTask runDelayed(Plugin plugin, Consumer<ScheduledTask> task, long delay, TimeUnit unit) {
            return async(plugin, task, Math.max(1L, unit.toMillis(delay)), 0);
        }

        @Override
        public ScheduledTask runAtFixedRate(Plugin plugin, Consumer<ScheduledTask> task, long initialDelay,
            long period, TimeUnit unit) {
            return async(plugin, task, Math.max(0L, unit.toMillis(initialDelay)), Math.max(50L, unit.toMillis(period)));
        }

        @Override
        public void cancelTasks(Plugin plugin) {
            scheduler().cancelTasks(plugin);
        }
    }

    public static final class OfEntity implements EntityScheduler {
        private final Entity entity;

        public OfEntity(Entity entity) {
            this.entity = entity;
        }

        private boolean valid() {
            return entity.isValid();
        }

        @Override
        public boolean execute(Plugin plugin, Runnable run, Runnable retired, long delay) {
            if (!valid()) {
                return false;
            }
            scheduler().runTaskLater(plugin, () -> {
                if (valid()) {
                    run.run();
                } else if (retired != null) {
                    retired.run();
                }
            }, Math.max(delay, 0L));
            return true;
        }

        private ScheduledTask guarded(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired, long delay,
            long period) {
            if (!valid()) {
                return null;
            }
            Consumer<ScheduledTask> body = handle -> {
                if (valid()) {
                    task.accept(handle);
                } else {
                    handle.cancel();
                    if (retired != null) {
                        retired.run();
                    }
                }
            };
            return sync(plugin, body, delay, period, retired);
        }

        @Override
        public ScheduledTask run(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired) {
            return guarded(plugin, task, retired, 0, 0);
        }

        @Override
        public ScheduledTask runDelayed(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired, long delay) {
            positive(delay, "Delay");
            return guarded(plugin, task, retired, delay, 0);
        }

        @Override
        public ScheduledTask runAtFixedRate(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired,
            long initialDelay, long period) {
            positive(initialDelay, "Initial delay");
            positive(period, "Period");
            return guarded(plugin, task, retired, initialDelay, period);
        }
    }
}
