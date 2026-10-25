package org.basaltmc.bukkit.item;

import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.TypedKey;
import io.papermc.paper.registry.set.RegistryKeySet;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import net.kyori.adventure.key.Key;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;

public final class KeySet<T extends Keyed> implements RegistryKeySet<T> {
    private final RegistryKey<T> registry;
    private final List<TypedKey<T>> keys = new ArrayList<>();

    public KeySet(RegistryKey<T> registry, Collection<String> ids) {
        this.registry = registry;
        for (String id : ids) {
            keys.add(TypedKey.create(registry, Key.key(id)));
        }
    }

    @Override
    public RegistryKey<T> registryKey() {
        return registry;
    }

    @Override
    public Collection<TypedKey<T>> values() {
        return List.copyOf(keys);
    }

    @Override
    public Collection<T> resolve(Registry<T> from) {
        List<T> out = new ArrayList<>();
        for (TypedKey<T> key : keys) {
            T found = from.get(new NamespacedKey(key.key().namespace(), key.key().value()));
            if (found != null) {
                out.add(found);
            }
        }
        return out;
    }

    @Override
    public boolean contains(TypedKey<T> key) {
        return keys.contains(key);
    }
}
