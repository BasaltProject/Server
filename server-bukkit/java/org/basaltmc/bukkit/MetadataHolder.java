package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.bukkit.metadata.MetadataValue;
import org.bukkit.plugin.Plugin;

/** The metadata map Bukkit objects carry, kept in memory for the life of the server. */
public final class MetadataHolder {
    private final Map<String, Map<Plugin, MetadataValue>> values = new HashMap<>();

    public synchronized void set(String key, MetadataValue value) {
        if (value == null) {
            throw new IllegalArgumentException("Value cannot be null");
        }
        if (value.getOwningPlugin() == null) {
            throw new IllegalArgumentException("Plugin cannot be null");
        }
        values.computeIfAbsent(key, k -> new java.util.WeakHashMap<>(1)).put(value.getOwningPlugin(), value);
    }

    public synchronized List<MetadataValue> get(String key) {
        Map<Plugin, MetadataValue> found = values.get(key);
        return found == null ? new ArrayList<>() : new ArrayList<>(found.values());
    }

    public synchronized boolean has(String key) {
        Map<Plugin, MetadataValue> found = values.get(key);
        return found != null && !found.isEmpty();
    }

    public synchronized void remove(String key, Plugin plugin) {
        if (plugin == null) {
            throw new IllegalArgumentException("Plugin cannot be null");
        }
        Map<Plugin, MetadataValue> found = values.get(key);
        if (found != null) {
            found.remove(plugin);
            if (found.isEmpty()) {
                values.remove(key);
            }
        }
    }

    /** Asks every lazy value of the plugin to compute again, as MetadataStore#invalidateAll does. */
    public synchronized void invalidateAll(Plugin plugin) {
        for (Map<Plugin, MetadataValue> found : values.values()) {
            MetadataValue value = found.get(plugin);
            if (value != null) {
                value.invalidate();
            }
        }
    }
}
