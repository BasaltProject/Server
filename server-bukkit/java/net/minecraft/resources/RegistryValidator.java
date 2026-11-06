// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import java.util.Map;
import net.minecraft.core.Registry;

public interface RegistryValidator<T> {
    RegistryValidator<?> NONE = (registry, errors) -> {
    };
    RegistryValidator<?> NON_EMPTY = (registry, errors) -> {
        if (registry.size() == 0) {
            errors.put(registry.key(), new IllegalStateException("Registry must be non-empty"));
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
