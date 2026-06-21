package org.basaltmc.bukkit;

import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.TypedKey;
import io.papermc.paper.registry.tag.Tag;
import io.papermc.paper.registry.tag.TagKey;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import net.kyori.adventure.key.Key;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;

/** A tag of a registry, held as the ids of its members. */
public final class BasaltRegistryTag<T extends Keyed> implements Tag<T> {
    private final RegistryKey<T> registry;
    private final TagKey<T> key;
    private final List<TypedKey<T>> members = new ArrayList<>();

    public BasaltRegistryTag(RegistryKey<T> registry, TagKey<T> key, Collection<String> ids) {
        this.registry = registry;
        this.key = key;
        for (String id : ids) {
            members.add(TypedKey.create(registry, Key.key(id)));
        }
    }

    @Override
    public TagKey<T> tagKey() {
        return key;
    }

    @Override
    public RegistryKey<T> registryKey() {
        return registry;
    }

    @Override
    public Collection<TypedKey<T>> values() {
        return List.copyOf(members);
    }

    @Override
    public Collection<T> resolve(Registry<T> from) {
        List<T> out = new ArrayList<>();
        for (TypedKey<T> member : members) {
            T found = from.get(new NamespacedKey(member.key().namespace(), member.key().value()));
            if (found != null) {
                out.add(found);
            }
        }
        return out;
    }

    @Override
    public boolean contains(TypedKey<T> key) {
        return members.contains(key);
    }
}
