// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;
import java.util.Map;
import net.minecraft.core.Registry;
@FunctionalInterface
public interface RegistryValidator<T> {
    RegistryValidator<?> NONE = (registry, loadingErrors) -> {
    };
    RegistryValidator<?> NON_EMPTY = (registry, loadingErrors) -> {
        if (registry.size() == 0) {
            loadingErrors.put(registry.key(), new IllegalStateException("Registry must be non-empty: " + registry.key().identifier()));
        }
    };
    @SuppressWarnings("unchecked")
    static <T> RegistryValidator<T> none() {
        return (RegistryValidator<T>) NONE;
    }
    @SuppressWarnings("unchecked")
    static <T> RegistryValidator<T> nonEmpty() {
        return (RegistryValidator<T>) NON_EMPTY;
    }
    void validate(Registry<T> registry, Map<ResourceKey<?>, Exception> loadingErrors);
}
