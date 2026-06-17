package org.basaltmc.bukkit.player;

import io.papermc.paper.threadedregions.scheduler.EntityScheduler;
import io.papermc.paper.threadedregions.scheduler.ScheduledTask;
import java.util.function.Consumer;
import org.bukkit.Bukkit;
import org.bukkit.entity.Entity;
import org.bukkit.plugin.Plugin;
import org.bukkit.scheduler.BukkitTask;

final class PlayerEntityScheduler implements EntityScheduler {
    private final Entity entity;

    PlayerEntityScheduler(Entity entity) {
        this.entity = entity;
    }

    private static final class Task implements ScheduledTask {
        private final Plugin plugin;
        private final boolean repeating;
        private volatile BukkitTask handle;
        private volatile boolean cancelled;
        private volatile boolean finished;

        Task(Plugin plugin, boolean repeating) {
            this.plugin = plugin;
            this.repeating = repeating;
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
            if (finished) {
                return CancelledState.ALREADY_EXECUTED;
            }
            if (cancelled) {
                return CancelledState.CANCELLED_ALREADY;
            }
            cancelled = true;
            BukkitTask held = handle;
            if (held != null) {
                held.cancel();
            }
            return CancelledState.CANCELLED_BY_CALLER;
        }

        @Override
        public ExecutionState getExecutionState() {
            return cancelled ? ExecutionState.CANCELLED : finished ? ExecutionState.FINISHED : ExecutionState.IDLE;
        }
    }

    private boolean gone() {
        return !entity.isValid();
    }

    @Override
    public boolean execute(Plugin plugin, Runnable run, Runnable retired, long delay) {
        if (gone()) {
            return false;
        }
        Bukkit.getScheduler().runTaskLater(plugin, () -> {
            if (gone()) {
                if (retired != null) {
                    retired.run();
                }
            } else {
                run.run();
            }
        }, Math.max(delay, 1L) - 1L);
        return true;
    }

    @Override
    public ScheduledTask run(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired) {
        return runDelayed(plugin, task, retired, 1L);
    }

    @Override
    public ScheduledTask runDelayed(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired, long delay) {
        if (gone()) {
            return null;
        }
        Task scheduled = new Task(plugin, false);
        scheduled.handle = Bukkit.getScheduler().runTaskLater(plugin, () -> {
            if (scheduled.cancelled) {
                return;
            }
            if (gone()) {
                scheduled.finished = true;
                if (retired != null) {
                    retired.run();
                }
                return;
            }
            task.accept(scheduled);
            scheduled.finished = true;
        }, Math.max(delay, 1L) - 1L);
        return scheduled;
    }

    @Override
    public ScheduledTask runAtFixedRate(Plugin plugin, Consumer<ScheduledTask> task, Runnable retired,
            long initialDelay, long period) {
        if (gone()) {
            return null;
        }
        Task scheduled = new Task(plugin, true);
        scheduled.handle = Bukkit.getScheduler().runTaskTimer(plugin, () -> {
            if (scheduled.cancelled) {
                return;
            }
            if (gone()) {
                scheduled.cancel();
                if (retired != null) {
                    retired.run();
                }
                return;
            }
            task.accept(scheduled);
        }, Math.max(initialDelay, 1L) - 1L, Math.max(period, 1L));
        return scheduled;
    }
}
