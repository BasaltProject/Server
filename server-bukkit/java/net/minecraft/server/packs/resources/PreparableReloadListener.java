// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
public interface PreparableReloadListener {
    CompletableFuture<Void> reload(SharedState currentReload, Executor taskExecutor, PreparationBarrier preparationBarrier, Executor reloadExecutor);
    default void prepareSharedState(SharedState currentReload) {
        return;
    }
    default String getName() {
        return getClass().getSimpleName();
    }
    public static final class StateKey<T> {
    }
    public static final class SharedState {
        private final ResourceManager manager;
        private final java.util.Map<StateKey<?>, Object> values = new java.util.IdentityHashMap<>();

        public SharedState(ResourceManager manager) {
            this.manager = manager;
            
        }
        public ResourceManager resourceManager() {
            return manager;
        }
        public <T> void set(StateKey<T> key, T value) {
            values.put(key, value);
        }
        public <T> T get(StateKey<T> key) {
            return (T) java.util.Objects.requireNonNull(values.get(key));
        }
    }
    public interface PreparationBarrier {
        <T> CompletableFuture<T> wait(T t);
    }
}
