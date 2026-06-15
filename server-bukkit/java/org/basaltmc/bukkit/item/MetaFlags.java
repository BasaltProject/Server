package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.bukkit.Material;
import org.bukkit.inventory.ItemFlag;

/** Item flags as the components the tooltip display hides. */
final class MetaFlags {
    private static final String kDisplay = "minecraft:tooltip_display";

    private MetaFlags() {}

    static List<String> componentsOf(ItemFlag flag) {
        switch (flag) {
            case HIDE_ENCHANTS:
                return List.of("minecraft:enchantments");
            case HIDE_ATTRIBUTES:
                return List.of("minecraft:attribute_modifiers");
            case HIDE_UNBREAKABLE:
                return List.of("minecraft:unbreakable");
            case HIDE_DESTROYS:
                return List.of("minecraft:can_break");
            case HIDE_PLACED_ON:
                return List.of("minecraft:can_place_on");
            case HIDE_ADDITIONAL_TOOLTIP:
                return List.of("minecraft:banner_patterns", "minecraft:bees", "minecraft:block_entity_data",
                    "minecraft:block_state", "minecraft:bundle_contents", "minecraft:charged_projectiles",
                    "minecraft:container", "minecraft:container_loot", "minecraft:firework_explosion",
                    "minecraft:fireworks", "minecraft:instrument", "minecraft:map_id", "minecraft:painting/variant",
                    "minecraft:pot_decorations", "minecraft:potion_contents", "minecraft:tropical_fish/pattern",
                    "minecraft:written_book_content");
            case HIDE_DYE:
                return List.of("minecraft:dyed_color");
            case HIDE_ARMOR_TRIM:
                return List.of("minecraft:trim");
            default:
                return List.of("minecraft:stored_enchantments");
        }
    }

    private static Set<String> hidden(Map<String, Object> c) {
        Map<String, Object> display = Nbt.map(c.get(kDisplay));
        Set<String> out = new LinkedHashSet<>();
        List<Object> list = display == null ? null : Nbt.list(display.get("hidden_components"));
        if (list != null) {
            for (Object name : list) {
                out.add(String.valueOf(name));
            }
        }
        return out;
    }

    private static void store(Map<String, Object> c, Set<String> hidden) {
        Map<String, Object> display = Nbt.map(c.get(kDisplay));
        if (hidden.isEmpty()) {
            if (display != null) {
                display.remove("hidden_components");
                if (display.isEmpty()) {
                    c.remove(kDisplay);
                }
            }
            return;
        }
        if (display == null) {
            display = Nbt.compound();
            c.put(kDisplay, display);
        }
        display.put("hidden_components", new ArrayList<Object>(hidden));
    }

    static void add(Map<String, Object> c, ItemFlag[] flags) {
        Set<String> hidden = hidden(c);
        for (ItemFlag flag : flags) {
            hidden.addAll(componentsOf(flag));
        }
        store(c, hidden);
    }

    static void remove(Map<String, Object> c, ItemFlag[] flags) {
        Set<String> hidden = hidden(c);
        for (ItemFlag flag : flags) {
            hidden.removeAll(componentsOf(flag));
        }
        store(c, hidden);
    }

    static Set<ItemFlag> all(Map<String, Object> c) {
        Set<String> hidden = hidden(c);
        Set<ItemFlag> out = EnumSet.noneOf(ItemFlag.class);
        for (ItemFlag flag : ItemFlag.values()) {
            if (hidden.containsAll(componentsOf(flag))) {
                out.add(flag);
            }
        }
        return out;
    }

    static boolean has(Map<String, Object> c, ItemFlag flag) {
        return hidden(c).containsAll(componentsOf(flag));
    }

    static Set<Material> blocks(Map<String, Object> c, String component) {
        Set<Material> out = new LinkedHashSet<>();
        Map<String, Object> predicates = Nbt.map(c.get(component));
        List<Object> list = predicates == null ? null : Nbt.list(predicates.get("predicates"));
        if (list != null) {
            for (Object item : list) {
                Map<String, Object> entry = Nbt.map(item);
                Object blocks = entry == null ? null : entry.get("blocks");
                if (blocks instanceof String id && !id.startsWith("#")) {
                    Material material = Material.matchMaterial(id);
                    if (material != null) {
                        out.add(material);
                    }
                }
            }
        }
        return out;
    }

    static void setBlocks(Map<String, Object> c, String component, Set<Material> blocks) {
        if (blocks == null || blocks.isEmpty()) {
            c.remove(component);
            return;
        }
        List<Object> predicates = new ArrayList<>();
        for (Material material : blocks) {
            Map<String, Object> entry = Nbt.compound();
            entry.put("blocks", material.getKey().toString());
            predicates.add(entry);
        }
        Map<String, Object> value = Nbt.compound();
        value.put("predicates", predicates);
        c.put(component, value);
    }
}
