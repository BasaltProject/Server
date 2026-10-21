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
        values.computeIfAbsent(key, k -> new HashMap<>()).put(value.getOwningPlugin(), value);
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
        Map<Plugin, MetadataValue> found = values.get(key);
        if (found != null) {
            found.remove(plugin);
        }
    }
}
