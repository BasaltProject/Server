// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import net.minecraft.util.Util;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.util.Unit;
public class SimpleReloadInstance<S> implements ReloadInstance {
    private final CompletableFuture<Unit> allPreparations = new CompletableFuture<>();
    private CompletableFuture<List<S>> allDone;
    private final java.util.Set<PreparableReloadListener> preparingListeners;
    private final int listenerCount;
    private final java.util.concurrent.atomic.AtomicInteger startedTasks = new java.util.concurrent.atomic.AtomicInteger();
    private final java.util.concurrent.atomic.AtomicInteger finishedTasks = new java.util.concurrent.atomic.AtomicInteger();
    private final java.util.concurrent.atomic.AtomicInteger startedReloads = new java.util.concurrent.atomic.AtomicInteger();
    private final java.util.concurrent.atomic.AtomicInteger finishedReloads = new java.util.concurrent.atomic.AtomicInteger();

    private static int weigh(int tasks, int reloads, int listeners) {
        return tasks * 2 + reloads * 2 + listeners;
    }

    public static ReloadInstance of(ResourceManager resourceManager, List<PreparableReloadListener> listeners, Executor taskExecutor, Executor mainThreadExecutor, CompletableFuture<Unit> initialTask) {
        SimpleReloadInstance<Void> instance = new SimpleReloadInstance<>(listeners);
        instance.startTasks(taskExecutor, mainThreadExecutor, resourceManager, listeners, StateFactory.SIMPLE, initialTask);
        return instance;
    }
    protected SimpleReloadInstance(List<PreparableReloadListener> listeners) {
        listenerCount = listeners.size();
        preparingListeners = new java.util.HashSet<>(listeners);
        
    }
    protected void startTasks(Executor taskExecutor, Executor mainThreadExecutor, ResourceManager resourceManager, List<PreparableReloadListener> listeners, StateFactory<S> stateFactory, CompletableFuture<?> initialTask) {
        allDone = prepareTasks(taskExecutor, mainThreadExecutor, resourceManager, listeners, stateFactory, initialTask);
    }
    protected CompletableFuture<List<S>> prepareTasks(Executor taskExecutor, Executor mainThreadExecutor, ResourceManager resourceManager, List<PreparableReloadListener> listeners, StateFactory<S> stateFactory, CompletableFuture<?> initialTask) {
        Executor countedTasks = work -> {
            startedTasks.incrementAndGet();
            taskExecutor.execute(() -> {
                work.run();
                finishedTasks.incrementAndGet();
            });
        };
        Executor countedReloads = work -> {
            startedReloads.incrementAndGet();
            mainThreadExecutor.execute(() -> {
                work.run();
                finishedReloads.incrementAndGet();
            });
        };
        startedTasks.incrementAndGet();
        initialTask.thenRun(finishedTasks::incrementAndGet);
        PreparableReloadListener.SharedState shared = new PreparableReloadListener.SharedState(resourceManager);
        listeners.forEach(listener -> listener.prepareSharedState(shared));
        CompletableFuture<?> previous = initialTask;
        List<CompletableFuture<S>> steps = new java.util.ArrayList<>();
        for (PreparableReloadListener listener : listeners) {
            CompletableFuture<?> gate = previous;
            PreparableReloadListener.PreparationBarrier barrier = new PreparableReloadListener.PreparationBarrier() {
                @Override
                public <T> CompletableFuture<T> wait(T value) {
                    mainThreadExecutor.execute(() -> {
                        preparingListeners.remove(listener);
                        if (preparingListeners.isEmpty()) {
                            allPreparations.complete(Unit.INSTANCE);
                        }
                    });
                    return allPreparations.thenCombine(gate, (a, b) -> value);
                }
            };
            CompletableFuture<S> step = stateFactory.create(shared, barrier, listener, countedTasks, countedReloads);
            steps.add(step);
            previous = step;
        }
        return Util.sequenceFailFast(steps);
    }
    public CompletableFuture<?> done() {
        return java.util.Objects.requireNonNull(allDone, "not started");
    }
    public float getActualProgress() {
        int prepared = listenerCount - preparingListeners.size();
        return (float) weigh(finishedTasks.get(), finishedReloads.get(), prepared) / weigh(startedTasks.get(), startedReloads.get(), listenerCount);
    }
    public static ReloadInstance create(ResourceManager resourceManager, List<PreparableReloadListener> listeners, Executor backgroundExecutor, Executor mainThreadExecutor, CompletableFuture<Unit> initialTask, boolean enableProfiling) {
        return enableProfiling ? ProfiledReloadInstance.of(resourceManager, listeners, backgroundExecutor, mainThreadExecutor, initialTask)
            : of(resourceManager, listeners, backgroundExecutor, mainThreadExecutor, initialTask);
    }
    protected interface StateFactory<S> {
        StateFactory<Void> SIMPLE = (shared, previous, listener, tasks, reloads) -> listener.reload(shared, tasks, previous, reloads);
        CompletableFuture<S> create(PreparableReloadListener.SharedState sharedState, PreparableReloadListener.PreparationBarrier previousStep, PreparableReloadListener listener, Executor taskExecutor, Executor reloadExecutor);
    }
}
