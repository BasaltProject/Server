// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import com.mojang.serialization.Lifecycle;
import java.util.Optional;
import net.minecraft.util.RandomSource;

public class DefaultedMappedRegistry<T> extends MappedRegistry<T> implements DefaultedRegistry<T> {
    private final Identifier defaultKey;

    public DefaultedMappedRegistry(String defaultKey, ResourceKey<? extends Registry<T>> key) {
        super(key);
        this.defaultKey = Identifier.parse(defaultKey);
    }

    @Override
    public Identifier getDefaultKey() {
        return defaultKey;
    }

    @Override
    public Identifier getKey(T value) {
        Identifier found = super.getKey(value);
        return found == null ? defaultKey : found;
    }

    @Override
    public T getValue(Identifier id) {
        T found = super.getValue(id);
        return found == null ? super.getValue(defaultKey) : found;
    }

    @Override
    public T getValue(ResourceKey<T> key) {
        T found = super.getValue(key);
        return found == null ? super.getValue(defaultKey) : found;
    }

    @Override
    public T byId(int id) {
        T found = super.byId(id);
        return found == null ? super.getValue(defaultKey) : found;
    }

    @Override
    public int getId(T value) {
        int found = super.getId(value);
        return found == -1 ? super.getId(super.getValue(defaultKey)) : found;
    }
    public DefaultedMappedRegistry(String defaultKey, ResourceKey<? extends Registry<T>> key, Lifecycle lifecycle, boolean intrusiveHolders) {
        super(key, lifecycle);
        this.defaultKey = Identifier.parse(defaultKey);
    }
    public Holder.Reference<T> register(ResourceKey<T> key, T value, RegistrationInfo registrationInfo) {
        Holder.Reference<T> holder = super.register(key, value, registrationInfo);
        return holder;
    }
    public Optional<T> getOptional(Identifier key) {
        return Optional.ofNullable(super.getValue(key));
    }
    public Optional<Holder.Reference<T>> getAny() {
        return super.getAny().or(() -> get(defaultKey));
    }
    public Optional<Holder.Reference<T>> getRandom(RandomSource random) {
        return super.getRandom(random).or(() -> get(defaultKey));
    }
}
