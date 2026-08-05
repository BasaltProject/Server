package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.datacomponent.DataComponentBuilder;
import io.papermc.paper.datacomponent.DataComponentType;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.item.ItemDefaults;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.inventory.ItemStack;

public final class ComponentOps {
    private ComponentOps() {}

    private static String name(DataComponentType type) {
        return type.getKey().getKey();
    }

    private static String full(DataComponentType type) {
        return type.getKey().toString();
    }

    private static boolean hasDefault(ItemStack stack, String name) {
        return ItemDefaults.of(stack.getType().getKey().toString()).containsKey("minecraft:" + name);
    }

    private static Object defaultOf(ItemStack stack, String name) {
        Object stored = ItemDefaults.of(stack.getType().getKey().toString()).get("minecraft:" + name);
        return stored == null ? null : Codecs.decode(name, Nbt.copy(stored));
    }

    private static boolean removed(Map<String, Object> components, String full) {
        return components.containsKey("!" + full);
    }

    @SuppressWarnings("unchecked")
    public static <T> T get(ItemStack stack, DataComponentType.Valued<T> type) {
        Map<String, Object> components = Items.componentsOf(stack);
        String full = full(type);
        if (components.containsKey(full)) {
            return (T) Codecs.decode(name(type), components.get(full));
        }
        if (!removed(components, full) && hasDefault(stack, name(type))) {
            return (T) defaultOf(stack, name(type));
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    public static <T> T decodeDefault(DataComponentType.Valued<T> type, Object stored) {
        return (T) Codecs.decode(name(type), Nbt.copy(stored));
    }

    public static boolean has(ItemStack stack, DataComponentType type) {
        Map<String, Object> components = Items.componentsOf(stack);
        String full = full(type);
        return components.containsKey(full) || !removed(components, full) && hasDefault(stack, name(type));
    }

    public static Set<DataComponentType> types(ItemStack stack) {
        Set<DataComponentType> out = new LinkedHashSet<>();
        for (DataComponentType type : ComponentType.list()) {
            if (has(stack, type)) {
                out.add(type);
            }
        }
        return out;
    }

    public static <T> void set(ItemStack stack, DataComponentType.Valued<T> type, T value) {
        if (value == null) {
            throw new NullPointerException("value");
        }
        Map<String, Object> components = Items.componentsOf(stack);
        components.put(full(type), Codecs.encode(name(type), value));
        components.remove("!" + full(type));
        touch(stack);
    }

    public static <T> void set(ItemStack stack, DataComponentType.Valued<T> type, DataComponentBuilder<T> builder) {
        set(stack, type, builder.build());
    }

    public static void set(ItemStack stack, DataComponentType.NonValued type) {
        Map<String, Object> components = Items.componentsOf(stack);
        components.put(full(type), Nbt.compound());
        components.remove("!" + full(type));
        touch(stack);
    }

    public static void unset(ItemStack stack, DataComponentType type) {
        Map<String, Object> components = Items.componentsOf(stack);
        components.remove(full(type));
        if (hasDefault(stack, name(type))) {
            components.put("!" + full(type), Nbt.compound());
        }
        touch(stack);
    }

    public static void reset(ItemStack stack, DataComponentType type) {
        Map<String, Object> components = Items.componentsOf(stack);
        components.remove(full(type));
        components.remove("!" + full(type));
        touch(stack);
    }

    public static boolean overridden(ItemStack stack, DataComponentType type) {
        Map<String, Object> components = Items.componentsOf(stack);
        return components.containsKey(full(type)) || removed(components, full(type));
    }

    public static void copy(ItemStack stack, ItemStack source, Predicate<DataComponentType> filter) {
        Map<String, Object> from = Items.componentsOf(source);
        Map<String, Object> into = Items.componentsOf(stack);
        for (DataComponentType type : ComponentType.list()) {
            if (!filter.test(type)) {
                continue;
            }
            String full = full(type);
            if (from.containsKey(full)) {
                into.put(full, Nbt.copy(from.get(full)));
                into.remove("!" + full);
            } else if (removed(from, full)) {
                into.remove(full);
                into.put("!" + full, Nbt.compound());
            }
        }
        touch(stack);
    }

    public static boolean matchesWithoutData(ItemStack stack, ItemStack other, Set<DataComponentType> exclude,
            boolean ignoreCount) {
        if (stack.getType() != other.getType() || !ignoreCount && stack.getAmount() != other.getAmount()) {
            return false;
        }
        Map<String, Object> left = Nbt.deepCopy(Items.componentsOf(stack));
        Map<String, Object> right = Nbt.deepCopy(Items.componentsOf(other));
        for (DataComponentType type : exclude) {
            left.remove(full(type));
            left.remove("!" + full(type));
            right.remove(full(type));
            right.remove("!" + full(type));
        }
        return Nbt.same(left, right);
    }

    private static void touch(ItemStack stack) {
        if (stack instanceof org.basaltmc.bukkit.BasaltItemStack own) {
            own.changed();
        }
    }
}
