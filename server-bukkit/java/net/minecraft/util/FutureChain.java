// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import org.slf4j.LoggerFactory;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.Consumer;
import org.slf4j.Logger;

public class FutureChain implements TaskChainer, AutoCloseable {
    private static final Logger kLogger = LoggerFactory.getLogger(FutureChain.class);
    private final Executor executor;
    private CompletableFuture<?> head = CompletableFuture.completedFuture(null);
    private volatile boolean closed;

    public FutureChain(Executor executor) {
        this.executor = executor;
    }

    @Override
    public <T> void append(CompletableFuture<T> preparation, Consumer<T> chainedTask) {
        head = head.thenCombine(preparation, (ignored, prepared) -> prepared).thenAcceptAsync(prepared -> {
            if (!closed) {
                chainedTask.accept(prepared);
            }
        }, executor).exceptionally(error -> {
            kLogger.error("Chained task failed", error);
            return null;
        });
    }

    @Override
    public void close() {
        closed = true;
    }
}
