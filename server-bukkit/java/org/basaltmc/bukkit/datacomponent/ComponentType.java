package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.datacomponent.DataComponentType;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.NamespacedKey;

public class ComponentType implements DataComponentType {
    private static final Set<String> kUnits = Set.of("unbreakable", "creative_slot_lock", "intangible_projectile",
        "glider", "waxed");
    private static final Set<String> kTransient = Set.of("creative_slot_lock", "additional_trade_cost",
        "map_post_processing");
    private static Map<String, ComponentType> all;

    private final NamespacedKey key;

    private ComponentType(NamespacedKey key) {
        this.key = key;
    }

    public static final class Valued<T> extends ComponentType implements DataComponentType.Valued<T> {
        private Valued(NamespacedKey key) {
            super(key);
        }
    }

    public static final class NonValued extends ComponentType implements DataComponentType.NonValued {
        private NonValued(NamespacedKey key) {
            super(key);
        }
    }

    public static synchronized Map<String, ComponentType> all() {
        if (all == null) {
            all = new LinkedHashMap<>();
            for (String id : WorldNatives.registryIds("data_component_type")) {
                NamespacedKey key = NamespacedKey.fromString(id);
                all.put(id, kUnits.contains(key.getKey()) ? new NonValued(key) : new Valued<>(key));
            }
        }
        return all;
    }

    public static List<DataComponentType> list() {
        return new ArrayList<>(all().values());
    }

    public static ComponentType of(String id) {
        return all().get(id);
    }

    public String id() {
        return key.toString();
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public boolean isPersistent() {
        return !kTransient.contains(key.getKey());
    }

    @Override
    public String toString() {
        return "DataComponentType[" + key + "]";
    }
}
