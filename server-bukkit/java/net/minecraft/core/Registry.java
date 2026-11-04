// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.List;
import com.mojang.serialization.DataResult;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.Keyable;
import java.util.Map;
import net.minecraft.core.component.DataComponentLookup;
import net.minecraft.tags.TagLoader;
import net.minecraft.util.RandomSource;

public interface Registry<T> extends IdMap<T>, HolderLookup.RegistryLookup<T>, Keyable {
    @Override
    ResourceKey<? extends Registry<T>> key();

    Identifier getKey(T value);

    Optional<ResourceKey<T>> getResourceKey(T value);

    T getValue(ResourceKey<T> key);

    T getValue(Identifier id);

    default T getValueOrThrow(ResourceKey<T> key) {
        T value = getValue(key);
        if (value == null) {
            throw new IllegalStateException("Missing key in " + key() + ": " + key);
        }
        return value;
    }

    default Optional<T> getOptional(Identifier id) {
        return Optional.ofNullable(getValue(id));
    }

    default Optional<T> getOptional(ResourceKey<T> key) {
        return Optional.ofNullable(getValue(key));
    }

    Optional<Holder.Reference<T>> get(Identifier id);

    @Override
    Optional<Holder.Reference<T>> get(ResourceKey<T> key);

    Optional<Holder.Reference<T>> get(int id);

    @Override
    default Holder.Reference<T> getOrThrow(ResourceKey<T> key) {
        return get(key).orElseThrow(() -> new IllegalStateException("Missing key in " + key() + ": " + key));
    }

    Set<Identifier> keySet();

    Set<ResourceKey<T>> registryKeySet();

    Holder<T> wrapAsHolder(T value);

    IdMap<Holder<T>> asHolderIdMap();

    @Override
    Optional<HolderSet.Named<T>> get(TagKey<T> tag);

    Stream<HolderSet.Named<T>> getTags();

    boolean containsKey(Identifier id);

    default Stream<T> stream() {
        return StreamSupport.stream(spliterator(), false);
    }
    default Codec<T> byNameCodec() {
        return Identifier.CODEC.flatXmap(
            id -> getOptional(id).map(DataResult::success).orElseGet(() -> DataResult.error(() -> "Unknown registry key in " + key() + ": " + id)),
            value -> getResourceKey(value).map(key -> DataResult.success(key.identifier())).orElseGet(() -> DataResult.error(() -> "Unknown registry element in " + key() + ":" + value)));
    }
    default Codec<Holder<T>> holderByNameCodec() {
        return Identifier.CODEC.flatXmap(
            id -> get(id).map(holder -> DataResult.success((Holder<T>) holder)).orElseGet(() -> DataResult.error(() -> "Unknown registry key in " + key() + ": " + id)),
            holder -> holder.unwrapKey().map(key -> DataResult.success(key.identifier())).orElseGet(() -> DataResult.error(() -> "Unknown registry element in " + key() + ":" + holder)));
    }
    default <U> Stream<U> keys(DynamicOps<U> ops) {
        return keySet().stream().map(id -> ops.createString(id.toString()));
    }
    default int getId(T thing) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.Registry#getId");
    }
    default Optional<RegistrationInfo> registrationInfo(ResourceKey<T> element) {
        return Optional.empty();
    }
    default Optional<Holder.Reference<T>> getAny() {
        return listElements().findFirst();
    }
    default Set<Map.Entry<ResourceKey<T>, T>> entrySet() {
        Set<Map.Entry<ResourceKey<T>, T>> entries = new java.util.LinkedHashSet<>();
        for (ResourceKey<T> key : registryKeySet()) {
            entries.add(Map.entry(key, getValue(key)));
        }
        return entries;
    }
    default Optional<Holder.Reference<T>> getRandom(RandomSource random) {
        List<Holder.Reference<T>> all = listElements().toList();
        return all.isEmpty() ? Optional.empty() : Optional.of(all.get(random.nextInt(all.size())));
    }
    default boolean containsKey(ResourceKey<T> key) {
        return getValue(key) != null;
    }
    static <T> T register(Registry<? super T> registry, String name, T value) {
        return register(registry, Identifier.parse(name), value);
    }
    static <V, T extends V> T register(Registry<V> registry, Identifier location, T value) {
        return register(registry, ResourceKey.create(registry.key(), location), value);
    }
    static <V, T extends V> T register(Registry<V> registry, ResourceKey<V> key, T value) {
        ((WritableRegistry<V>) registry).register(key, value);
        return value;
    }
    static <R, T extends R> Holder.Reference<T> registerForHolder(Registry<R> registry, ResourceKey<R> key, T value) {
        return (Holder.Reference<T>) ((WritableRegistry<R>) registry).register(key, value);
    }
    static <R, T extends R> Holder.Reference<T> registerForHolder(Registry<R> registry, Identifier location, T value) {
        return registerForHolder(registry, ResourceKey.create(registry.key(), location), value);
    }
    default Registry<T> freeze() {
        return this;
    }
    default Holder.Reference<T> createIntrusiveHolder(T value) {
        return Holder.Reference.createIntrusive(this, value);
    }
    default Iterable<Holder<T>> getTagOrEmpty(TagKey<T> id) {
        return get(id).<Iterable<Holder<T>>>map(set -> set).orElse(List.of());
    }
    default PendingTags<T> prepareTagReload(TagLoader.LoadResult<T> tags) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.Registry#prepareTagReload");
    }
    default DataComponentLookup<T> componentLookup() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.Registry#componentLookup");
    }
    public interface PendingTags<T> {
        ResourceKey<? extends Registry<? extends T>> key();
        HolderLookup.RegistryLookup<T> lookup();
        void apply();
        int size();
    }
}
