// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;

import java.util.function.BooleanSupplier;
import net.minecraft.util.profiling.metrics.ProfilerMeasured;
import org.basaltmc.bukkit.Unsupported;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.Supplier;
import net.minecraft.CrashReport;
import net.minecraft.util.profiling.metrics.MetricSampler;
public abstract class BlockableEventLoop<R extends Runnable> implements TaskScheduler<R>, ProfilerMeasured, Executor {
    private final java.util.Queue<Runnable> pending = new java.util.concurrent.ConcurrentLinkedQueue<>();
    private String name = "event loop";
    private Thread loopThread;

    protected BlockableEventLoop() {
    }
    public void managedBlock(BooleanSupplier condition) {
        while (!condition.getAsBoolean()) {
            if (!pollTask()) {
                Thread.onSpinWait();
            }
        }
    }
    public static final long BLOCK_TIME_NANOS = 100000L;
    protected BlockableEventLoop(String name, boolean propagatesCrashes) {
        this.name = name == null ? this.name : name;
    }
    protected boolean shouldRun(R task) {
        return true;
    }
    public boolean isSameThread() {
        return Thread.currentThread() == getRunningThread();
    }
    protected Thread getRunningThread() {
        return loopThread != null ? loopThread : Thread.currentThread();
    }
    protected boolean scheduleExecutables() {
        return true;
    }
    public int getPendingTasksCount() {
        return pending.size();
    }
    public String name() {
        return name;
    }
    public <V> CompletableFuture<V> submit(Supplier<V> supplier) {
        CompletableFuture<V> future = new CompletableFuture<>();
        execute(() -> {
            try {
        future.complete(supplier.get());
            } catch (Throwable t) {
        future.completeExceptionally(t);
            }
        });
        return future;
    }
    public CompletableFuture<Void> submit(Runnable runnable) {
        return submit(() -> {
            runnable.run();
            return null;
        });
    }
    public void executeBlocking(Runnable runnable) {
        if (isSameThread()) {
            runnable.run();
        } else {
            submit(runnable).join();
        }
    }
    public void schedule(R r) {
        pending.add(r);
    }
    public void execute(Runnable command) {
        pending.add(command);
    }
    public void executeIfPossible(Runnable command) {
        execute(command);
    }
    protected void dropAllTasks() {
        pending.clear();
    }
    protected void runAllTasks() {
        while (pollTask()) {
        }
    }
    protected boolean shouldRunAllTasks() {
        return !pending.isEmpty();
    }
    protected boolean pollTask() {
        Runnable task = pending.poll();
        if (task == null) {
            return false;
        }
        task.run();
        return true;
    }
    protected void waitForTasks() {
        java.util.concurrent.locks.LockSupport.parkNanos("waiting for tasks", BLOCK_TIME_NANOS);
    }
    protected void doRunTask(R task) {
        task.run();
    }
    public List<MetricSampler> profiledMetrics() {
        return List.of();
    }
    public static boolean isNonRecoverable(Throwable t) {
        return t instanceof VirtualMachineError && !(t instanceof StackOverflowError);
    }
    public void delayCrash(CrashReport crashReport) {
        throw new RuntimeException(String.valueOf(crashReport));
    }
    public static void relayDelayCrash(CrashReport crashReport) {
    }
}
