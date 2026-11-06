// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.Consumer;
public interface TaskScheduler<R extends Runnable> extends AutoCloseable {
    String name();
    void schedule(R r);
    default void close() {
    }
    R wrapRunnable(Runnable runnable);
    default <Source> CompletableFuture<Source> scheduleWithResult(Consumer<CompletableFuture<Source>> futureConsumer) {
        CompletableFuture<Source> future = new CompletableFuture<>();
        schedule(wrapRunnable(() -> futureConsumer.accept(future)));
        return future;
    }
    static TaskScheduler<Runnable> wrapExecutor(String name, Executor executor) {
        return new TaskScheduler<>() {
            public String name() {
                return name;
            }
            public void schedule(Runnable runnable) {
                executor.execute(runnable);
            }
            public Runnable wrapRunnable(Runnable runnable) {
                return runnable;
            }
            public String toString() {
                return name;
            }
        };
    }
}
