// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.util.Unit;
public class ProfiledReloadInstance extends SimpleReloadInstance<ProfiledReloadInstance.State> {
    public static ReloadInstance of(ResourceManager resourceManager, List<PreparableReloadListener> listeners, Executor taskExecutor, Executor mainThreadExecutor, CompletableFuture<Unit> initialTask) {
        ProfiledReloadInstance instance = new ProfiledReloadInstance(listeners);
        instance.startTasks(taskExecutor, mainThreadExecutor, resourceManager, listeners, (shared, previous, listener, parentTasks, parentReloads) -> {
            AtomicLong prepNanos = new AtomicLong();
            AtomicLong prepCount = new AtomicLong();
            AtomicLong reloadNanos = new AtomicLong();
            AtomicLong reloadCount = new AtomicLong();
            return listener.reload(shared, timed(parentTasks, prepNanos, prepCount), previous, timed(parentReloads, reloadNanos, reloadCount))
                .thenApplyAsync(done -> new State(listener.getName(), prepNanos, prepCount, reloadNanos, reloadCount), mainThreadExecutor);
        }, initialTask);
        return instance;
    }
    private static Executor timed(Executor parent, AtomicLong nanos, AtomicLong count) {
        return work -> parent.execute(() -> {
            long begin = System.nanoTime();
            work.run();
            nanos.addAndGet(System.nanoTime() - begin);
            count.incrementAndGet();
        });
    }

    private ProfiledReloadInstance(List<PreparableReloadListener> listeners) {
        super(listeners);
    }
    protected CompletableFuture<List<State>> prepareTasks(Executor taskExecutor, Executor mainThreadExecutor, ResourceManager resourceManager, List<PreparableReloadListener> listeners, SimpleReloadInstance.StateFactory<State> stateFactory, CompletableFuture<?> initialTask) {
        return super.prepareTasks(taskExecutor, mainThreadExecutor, resourceManager, listeners, stateFactory, initialTask);
    }
    public static record State(String name, AtomicLong preparationNanos, AtomicLong preparationCount, AtomicLong reloadNanos, AtomicLong reloadCount) {
    }
}
