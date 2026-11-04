// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.Consumer;
public interface TaskScheduler<R extends Runnable> extends AutoCloseable {
    default String name() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.thread.TaskScheduler#name");
    }
    default void schedule(R r) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.thread.TaskScheduler#schedule");
    }
    default void close() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.thread.TaskScheduler#close");
    }
    default R wrapRunnable(Runnable runnable) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.thread.TaskScheduler#wrapRunnable");
    }
    default <Source> CompletableFuture<Source> scheduleWithResult(Consumer<CompletableFuture<Source>> futureConsumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.thread.TaskScheduler#scheduleWithResult");
    }
    static TaskScheduler<Runnable> wrapExecutor(String name, Executor executor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.thread.TaskScheduler#wrapExecutor");
    }
}
