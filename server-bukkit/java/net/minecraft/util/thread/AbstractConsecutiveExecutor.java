// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;

import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicReference;
import net.minecraft.util.profiling.metrics.MetricSampler;
import net.minecraft.util.profiling.metrics.ProfilerMeasured;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public abstract class AbstractConsecutiveExecutor<T extends Runnable> implements Runnable, TaskScheduler<T>, ProfilerMeasured {
    private static final Logger kLogger = LoggerFactory.getLogger(AbstractConsecutiveExecutor.class);
    private final AtomicReference<Status> status = new AtomicReference<>(Status.SLEEPING);
    private final StrictQueue<T> queue;
    private final Executor executor;
    private final String name;

    public AbstractConsecutiveExecutor(StrictQueue<T> queue, Executor executor, String name) {
        this.executor = executor;
        this.queue = queue;
        this.name = name;
    }

    @Override
    public void close() {
        status.set(Status.CLOSED);
    }

    private boolean pollTask() {
        if (status.get() != Status.RUNNING) {
            return false;
        }
        Runnable next = queue.pop();
        if (next == null) {
            return false;
        }
        String previous = Thread.currentThread().getName();
        Thread.currentThread().setName(name);
        try {
            next.run();
        } finally {
            Thread.currentThread().setName(previous);
        }
        return true;
    }

    @Override
    public void run() {
        try {
            pollTask();
        } finally {
            status.compareAndSet(Status.RUNNING, Status.SLEEPING);
            requeue();
        }
    }

    public void runAll() {
        try {
            while (pollTask()) {
                continue;
            }
        } finally {
            status.compareAndSet(Status.RUNNING, Status.SLEEPING);
            requeue();
        }
    }

    @Override
    public void schedule(T task) {
        queue.push(task);
        requeue();
    }

    private void requeue() {
        if (status.get() == Status.CLOSED || queue.isEmpty() || !status.compareAndSet(Status.SLEEPING, Status.RUNNING)) {
            return;
        }
        try {
            executor.execute(this);
        } catch (RejectedExecutionException first) {
            try {
                executor.execute(this);
            } catch (RejectedExecutionException second) {
                kLogger.error("Could not schedule ConsecutiveExecutor", second);
            }
        }
    }

    public int size() {
        return queue.size();
    }

    public boolean hasWork() {
        return status.get() == Status.RUNNING && !queue.isEmpty();
    }

    @Override
    public String toString() {
        return name + " " + status.get() + " " + queue.isEmpty();
    }

    @Override
    public String name() {
        return name;
    }

    @Override
    public List<MetricSampler> profiledMetrics() {
        return List.of();
    }

    static enum Status {
        SLEEPING, RUNNING, CLOSED;
    }
}
