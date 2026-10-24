package net.minecraft.resources;

import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.Registry;

public class ResourceKey<T> implements Comparable<ResourceKey<?>> {
    private static final Map<String, ResourceKey<?>> kValues = new ConcurrentHashMap<>();
    private final Identifier registryName;
    private final Identifier identifier;

    private ResourceKey(Identifier registryName, Identifier identifier) {
        this.registryName = registryName;
        this.identifier = identifier;
    }

    @SuppressWarnings("unchecked")
    public static <T> ResourceKey<T> create(ResourceKey<? extends Registry<T>> registryName, Identifier location) {
        return (ResourceKey<T>) org.basaltmc.bukkit.facade.Memo.get(kValues, registryName.identifier + ":" + location,
            ignored -> new ResourceKey<>(registryName.identifier, location));
    }

    @SuppressWarnings("unchecked")
    public static <T> ResourceKey<Registry<T>> createRegistryKey(Identifier registryName) {
        return (ResourceKey<Registry<T>>) org.basaltmc.bukkit.facade.Memo.get(kValues, "minecraft:root:" + registryName,
            ignored -> new ResourceKey<>(Identifier.withDefaultNamespace("root"), registryName));
    }

    public boolean isFor(ResourceKey<? extends Registry<?>> registry) {
        return registryName.equals(registry.identifier);
    }

    public Identifier registry() {
        return registryName;
    }

    public Identifier identifier() {
        return identifier;
    }

    @Override
    public int compareTo(ResourceKey<?> other) {
        int result = registryName.compareTo(other.registryName);
        return result != 0 ? result : identifier.compareTo(other.identifier);
    }

    @Override
    public boolean equals(Object other) {
        return this == other;
    }

    @Override
    public int hashCode() {
        return Objects.hash(registryName, identifier);
    }

    @Override
    public String toString() {
        return "ResourceKey[" + registryName + " / " + identifier + "]";
    }
}
