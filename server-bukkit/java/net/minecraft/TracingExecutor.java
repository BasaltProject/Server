// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
public record TracingExecutor(ExecutorService service) implements Executor {
    public Executor forName(String name) {
        return service;
    }
    public void execute(Runnable command) {
        service.execute(command);
    }
    public void shutdownAndAwait(long timeout, TimeUnit unit) {
        service.shutdown();
        boolean terminated;
        try {
            terminated = service.awaitTermination(timeout, unit);
        } catch (InterruptedException interrupted) {
            terminated = false;
        }
        if (!terminated) {
            service.shutdownNow();
        }
    }
}
