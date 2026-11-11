// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import org.slf4j.LoggerFactory;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.Consumer;
import org.slf4j.Logger;

public interface TaskChainer {
    Logger LOGGER = LoggerFactory.getLogger(TaskChainer.class);

    static TaskChainer immediate(Executor executor) {
        return new TaskChainer() {
            @Override
            public <T> void append(CompletableFuture<T> preparation, Consumer<T> chainedTask) {
                preparation.thenAcceptAsync(chainedTask, executor).exceptionally(error -> {
                    LOGGER.error("Task failed", error);
                    return null;
                });
            }
        };
    }

    default void append(Runnable task) {
        append(CompletableFuture.completedFuture(null), ignored -> task.run());
    }

    <T> void append(CompletableFuture<T> preparation, Consumer<T> chainedTask);
}
