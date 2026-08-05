package org.basaltmc.bukkit.item;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.WorldNatives;

public final class TagSets {
    private static final Map<String, Map<String, Set<String>>> cache = new HashMap<>();

    private TagSets() {}

    public static synchronized Set<String> members(String registry, String tag) {
        Map<String, Set<String>> byTag = cache.get(registry);
        if (byTag == null) {
            byTag = new HashMap<>();
            String[] flat = registry.equals("enchantment") ? InventoryNatives.registryTags(registry) : WorldNatives.tags(registry);
            for (int at = 0; at + 1 < flat.length; at += 2) {
                Set<String> members = new HashSet<>();
                if (!flat[at + 1].isEmpty()) {
                    for (String member : flat[at + 1].split("\\|")) {
                        members.add(member);
                    }
                }
                byTag.put(flat[at], members);
            }
            cache.put(registry, byTag);
        }
        Set<String> found = byTag.get(tag.startsWith("#") ? tag.substring(1) : tag);
        return found == null ? Set.of() : found;
    }

    /** A holder set in datapack form: `#tag`, one id, or a list of ids; the members of the registry it names. */
    public static Set<String> resolve(String registry, Object spec) {
        Set<String> out = new HashSet<>();
        if (spec instanceof String text) {
            if (text.startsWith("#")) {
                out.addAll(members(registry, text));
            } else {
                out.add(text.indexOf(':') < 0 ? "minecraft:" + text : text);
            }
        } else if (spec instanceof java.util.List<?> list) {
            for (Object entry : list) {
                out.addAll(resolve(registry, entry));
            }
        }
        return out;
    }
}
