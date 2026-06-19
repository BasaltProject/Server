package org.basaltmc.bukkit.tile;

import java.util.AbstractMap;
import java.util.Map;
import java.util.Set;

/** The data of a block entity seen as the component map of the matching item, so the item metas can edit it. */
final class ComponentView extends AbstractMap<String, Object> {
    private final Map<String, Object> data;
    private final Map<String, String> aliases;

    ComponentView(Map<String, Object> data, Map<String, String> aliases) {
        this.data = data;
        this.aliases = aliases;
    }

    private String key(Object component) {
        String name = String.valueOf(component);
        String alias = aliases.get(name);
        if (alias != null) {
            return alias;
        }
        return name.startsWith("minecraft:") ? name.substring("minecraft:".length()) : name;
    }

    @Override
    public Object get(Object key) {
        return data.get(key(key));
    }

    @Override
    public boolean containsKey(Object key) {
        return data.containsKey(key(key));
    }

    @Override
    public Object put(String key, Object value) {
        return data.put(key(key), value);
    }

    @Override
    public Object remove(Object key) {
        return data.remove(key(key));
    }

    @Override
    public Set<Entry<String, Object>> entrySet() {
        return data.entrySet();
    }
}
