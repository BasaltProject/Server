// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.Registry;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.function.UnaryOperator;
import net.minecraft.network.codec.StreamCodec;

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
    public static final StreamCodec<ByteBuf, ResourceKey<? extends Registry<?>>> REGISTRY_STREAM_CODEC =
        Identifier.STREAM_CODEC.map(id -> (ResourceKey<? extends Registry<?>>) (ResourceKey<?>) createRegistryKey(id), ResourceKey::identifier);
    public static <T> Codec<ResourceKey<T>> codec(ResourceKey<? extends Registry<T>> registryName) {
        return Identifier.CODEC.xmap(id -> create(registryName, id), ResourceKey::identifier);
    }
    public static <T> StreamCodec<ByteBuf, ResourceKey<T>> streamCodec(ResourceKey<? extends Registry<T>> registryName) {
        return Identifier.STREAM_CODEC.map(id -> create(registryName, id), ResourceKey::identifier);
    }
    @SuppressWarnings("unchecked")
    public <E> Optional<ResourceKey<E>> cast(ResourceKey<? extends Registry<E>> registry) {
        return isFor(registry) ? Optional.of((ResourceKey<E>) this) : Optional.empty();
    }
    public <E> ResourceKey<E> dependent(ResourceKey<? extends Registry<E>> registryKey, String suffix) {
        return create(registryKey, identifier.withSuffix(suffix));
    }
    public <E> ResourceKey<E> dependent(ResourceKey<? extends Registry<E>> registryKey, UnaryOperator<String> decoration) {
        return create(registryKey, identifier.withPath(decoration));
    }
    public ResourceKey<Registry<T>> registryKey() {
        return createRegistryKey(registryName);
    }
    static record InternKey(Identifier registry, Identifier identifier) {
    }
}
