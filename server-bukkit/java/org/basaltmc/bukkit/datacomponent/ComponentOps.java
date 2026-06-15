package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.datacomponent.DataComponentBuilder;
import io.papermc.paper.datacomponent.DataComponentType;
import io.papermc.paper.inventory.ItemRarity;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.item.ItemData;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.inventory.ItemStack;

/** The Paper data component calls on an item, over the component map the stack keeps. */
public final class ComponentOps {
    private ComponentOps() {}

    private static String name(DataComponentType type) {
        return type.getKey().getKey();
    }

    private static String full(DataComponentType type) {
        return type.getKey().toString();
    }

    private static boolean hasDefault(ItemStack stack, String name) {
        ItemData data = ItemData.of(stack.getType().getKey().toString());
        switch (name) {
            case "max_stack_size", "repair_cost", "lore", "enchantments", "rarity", "item_name", "item_model",
                "tooltip_display":
                return true;
            case "max_damage", "damage":
                return data.maxDamage() > 0;
            default:
                return false;
        }
    }

    private static Object defaultOf(ItemStack stack, String name) {
        String id = stack.getType().getKey().toString();
        switch (name) {
            case "max_stack_size":
                return ItemData.of(id).maxStackSize();
            case "max_damage":
                return ItemData.of(id).maxDamage();
            case "damage", "repair_cost":
                return 0;
            case "lore":
                return new PaperValues.Lore(java.util.List.of());
            case "enchantments":
                return new PaperValues.Enchants(Map.of());
            case "rarity":
                return ItemRarity.COMMON;
            case "item_name":
                return Component.translatable(stack.getType().translationKey());
            case "item_model":
                return net.kyori.adventure.key.Key.key(id);
            case "tooltip_display":
                return new PaperValues.Tooltip(false, Set.of());
            default:
                return null;
        }
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
