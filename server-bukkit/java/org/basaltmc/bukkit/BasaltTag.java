package org.basaltmc.bukkit;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import org.bukkit.Keyed;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Tag;
import org.bukkit.entity.EntityType;

public final class BasaltTag<T extends Keyed> implements Tag<T> {
    private static final Map<String, Map<NamespacedKey, BasaltTag<?>>> cache = new HashMap<>();

    private final NamespacedKey key;
    private final Set<T> values;

    private BasaltTag(NamespacedKey key, Set<T> values) {
        this.key = key;
        this.values = values;
    }

    @SuppressWarnings("unchecked")
    static synchronized <T extends Keyed> Map<NamespacedKey, BasaltTag<T>> all(String registry, Class<T> type) {
        Map<NamespacedKey, BasaltTag<?>> found = cache.get(registry);
        if (found == null) {
            found = new HashMap<>();
            String[] flat = WorldNatives.tags(registry);
            for (int i = 0; i + 1 < flat.length; i += 2) {
                Set<Keyed> members = new HashSet<>();
                if (!flat[i + 1].isEmpty()) {
                    for (String id : flat[i + 1].split("\\|")) {
                        Keyed member = member(registry, id);
                        if (member != null) {
                            members.add(member);
                        }
                    }
                }
                NamespacedKey key = NamespacedKey.fromString(flat[i]);
                found.put(key, new BasaltTag<>(key, members));
            }
            cache.put(registry, found);
        }
        return (Map<NamespacedKey, BasaltTag<T>>) (Map<?, ?>) found;
    }

    private static Keyed member(String registry, String id) {
        switch (registry) {
            case Tag.REGISTRY_BLOCKS, Tag.REGISTRY_ITEMS:
                return Material.matchMaterial(id);
            case Tag.REGISTRY_ENTITY_TYPES:
                EntityType type = EntityCore.typeOf(id);
                return type == EntityType.UNKNOWN ? null : type;
            default:
                return null;
        }
    }

    @Override
    public boolean isTagged(T item) {
        return values.contains(item);
    }

    @Override
    public Set<T> getValues() {
        return Set.copyOf(values);
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }
}
