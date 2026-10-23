package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.set.RegistryKeySet;
import io.papermc.paper.registry.tag.Tag;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.key.Key;
import org.basaltmc.bukkit.BasaltRegistryTag;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.KeySet;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.inventory.ItemStack;

/** Small conversions the component specs share. */
final class Wrap {
    private Wrap() {}

    static Key key(Object stored) {
        return stored == null ? null : Key.key(String.valueOf(stored));
    }

    static String id(Key key) {
        return key.asString();
    }

    static NamespacedKey bukkit(Key key) {
        return new NamespacedKey(key.namespace(), key.value());
    }

    static void put(Map<String, Object> out, String name, Object value) {
        if (value != null) {
            out.put(name, value);
        }
    }

    static void putKey(Map<String, Object> out, String name, Key key) {
        if (key != null) {
            out.put(name, key.asString());
        }
    }

    static Object bool(boolean value) {
        return Nbt.bool(value);
    }

    static <T extends Keyed> RegistryKeySet<T> set(RegistryKey<T> registry, Object stored) {
        if (stored instanceof String text && text.startsWith("#")) {
            Key tag = Key.key(text.substring(1));
            return new BasaltRegistryTag<>(registry, registry.tagKey(tag), List.of());
        }
        List<String> ids = new ArrayList<>();
        List<Object> list = Nbt.list(stored);
        if (list != null) {
            for (Object id : list) {
                ids.add(String.valueOf(id));
            }
        } else if (stored != null) {
            ids.add(String.valueOf(stored));
        }
        return new KeySet<>(registry, ids);
    }

    static Object setNbt(RegistryKeySet<?> set) {
        if (set instanceof Tag<?> tag) {
            return "#" + tag.tagKey().key().asString();
        }
        List<Object> ids = new ArrayList<>();
        for (var key : set.values()) {
            ids.add(key.key().asString());
        }
        return ids;
    }

    static <T extends Keyed> T lookup(Registry<T> registry, Object stored) {
        return stored == null ? null : registry.get(NamespacedKey.fromString(String.valueOf(stored)));
    }

    static Object stack(ItemStack stack) {
        return Convert.stack(stack);
    }

    static ItemStack stack(Object stored) {
        Map<String, Object> map = Nbt.map(stored);
        if (map == null && stored instanceof String id) {
            map = Nbt.compound();
            map.put("id", id);
        }
        return map == null ? null : Convert.stack(map);
    }

    static List<Object> stackList(List<?> stacks) {
        List<Object> out = new ArrayList<>();
        for (Object stack : stacks) {
            out.add(stack((ItemStack) stack));
        }
        return out;
    }

    static List<Object> stacksOf(Object stored) {
        List<Object> out = new ArrayList<>();
        List<Object> list = Nbt.list(stored);
        if (list != null) {
            for (Object entry : list) {
                ItemStack stack = stack(entry);
                if (stack != null) {
                    out.add(stack);
                }
            }
        }
        return out;
    }
}
