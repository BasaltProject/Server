package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

/** The `id[component=value,...]` text form of an item, with values in SNBT. */
final class ItemParser {
    private ItemParser() {}

    static ItemStack parse(String input) {
        String text = input.strip();
        int open = text.indexOf('[');
        String id = open < 0 ? text : text.substring(0, open);
        Material material = Material.matchMaterial(id);
        if (material == null || !material.isItem()) {
            throw new IllegalArgumentException("Unknown item " + id);
        }
        Map<String, Object> components = Nbt.compound();
        if (open >= 0) {
            if (!text.endsWith("]")) {
                throw new IllegalArgumentException("Unterminated components in " + input);
            }
            for (String entry : split(text.substring(open + 1, text.length() - 1))) {
                int equals = entry.indexOf('=');
                if (equals < 0) {
                    throw new IllegalArgumentException("Expected '=' in " + entry);
                }
                String name = entry.substring(0, equals).strip();
                if (name.startsWith("!")) {
                    continue;
                }
                if (name.indexOf(':') < 0) {
                    name = "minecraft:" + name;
                }
                Object value = value(entry.substring(equals + 1).strip());
                components.put(name, textual(name, value));
            }
        }
        return new BasaltItemStack(material, 1, components);
    }

    static Object value(String snbt) {
        byte[] bytes = InventoryNatives.parseSnbt(snbt);
        Map<String, Object> wrapped = bytes == null ? null : Nbt.read(bytes);
        if (wrapped == null || !wrapped.containsKey("value")) {
            throw new IllegalArgumentException("Invalid NBT " + snbt);
        }
        return wrapped.get("value");
    }

    private static Object textual(String name, Object value) {
        switch (name) {
            case "minecraft:custom_name", "minecraft:item_name":
                return jsonText(value);
            case "minecraft:lore":
                if (value instanceof List<?> list) {
                    List<Object> out = new ArrayList<>();
                    for (Object line : list) {
                        out.add(jsonText(line));
                    }
                    return out;
                }
                return value;
            default:
                return value;
        }
    }

    private static Object jsonText(Object value) {
        if (value instanceof String text && (text.startsWith("{") || text.startsWith("[") || text.startsWith("\""))) {
            try {
                return Texts.toNbt(Texts.fromJson(text));
            } catch (RuntimeException e) {
                return value;
            }
        }
        return value;
    }

    private static List<String> split(String body) {
        List<String> out = new ArrayList<>();
        int depth = 0;
        char quote = 0;
        int start = 0;
        for (int at = 0; at < body.length(); at++) {
            char ch = body.charAt(at);
            if (quote != 0) {
                if (ch == '\\') {
                    at++;
                } else if (ch == quote) {
                    quote = 0;
                }
            } else if (ch == '"' || ch == '\'') {
                quote = ch;
            } else if (ch == '{' || ch == '[') {
                depth++;
            } else if (ch == '}' || ch == ']') {
                depth--;
            } else if (ch == ',' && depth == 0) {
                out.add(body.substring(start, at));
                start = at + 1;
            }
        }
        if (start < body.length() && !body.substring(start).isBlank()) {
            out.add(body.substring(start));
        }
        return out;
    }
}
