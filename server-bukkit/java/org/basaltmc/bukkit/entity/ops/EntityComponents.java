package org.basaltmc.bukkit.entity.ops;

import java.util.Locale;
import java.util.Map;
import java.util.Set;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.DyeColor;

/** The data components an entity answers for: the variants and colours it saves, in the form the item codecs read. */
final class EntityComponents {
    private static final String[] kRabbits = {"brown", "white", "black", "black_and_white", "gold", "salt_and_pepper"};
    private static final String[] kParrots = {"red_blue", "blue", "green", "yellow_blue", "gray"};
    private static final String[] kAxolotls = {"lucy", "wild", "gold", "cyan", "blue"};
    private static final String[] kHorses = {"white", "creamy", "chestnut", "brown", "black", "gray", "dark_brown"};
    private static final String[] kLlamas = {"creamy", "white", "brown", "gray"};

    private EntityComponents() {}

    private static Object stored(EntityCore core, String key) {
        return EntityNbt.compound(core, "").get(key);
    }

    private static String indexed(Object value, String[] names, String fallback) {
        int at = value instanceof Number number ? number.intValue() : -1;
        if (at == 99) {
            return "killer_bunny";
        }
        return at >= 0 && at < names.length ? names[at] : fallback;
    }

    private static String dye(Object value) {
        return value instanceof Number number
            ? DyeColor.getByWoolData((byte) number.intValue()).name().toLowerCase(Locale.ROOT) : null;
    }

    private static final Map<String, Set<String>> kTypes = Map.ofEntries(
        Map.entry("wolf_variant", Set.of("wolf")), Map.entry("wolf_sound_variant", Set.of("wolf")),
        Map.entry("wolf_collar", Set.of("wolf")), Map.entry("cat_variant", Set.of("cat")),
        Map.entry("cat_sound_variant", Set.of("cat")), Map.entry("cat_collar", Set.of("cat")),
        Map.entry("frog_variant", Set.of("frog")), Map.entry("pig_variant", Set.of("pig")),
        Map.entry("pig_sound_variant", Set.of("pig")), Map.entry("cow_variant", Set.of("cow")),
        Map.entry("cow_sound_variant", Set.of("cow")), Map.entry("chicken_variant", Set.of("chicken")),
        Map.entry("chicken_sound_variant", Set.of("chicken")), Map.entry("painting_variant", Set.of("painting")),
        Map.entry("zombie_nautilus_variant", Set.of("zombie_nautilus")), Map.entry("salmon_size", Set.of("salmon")),
        Map.entry("fox_variant", Set.of("fox")), Map.entry("rabbit_variant", Set.of("rabbit")),
        Map.entry("parrot_variant", Set.of("parrot")), Map.entry("mooshroom_variant", Set.of("mooshroom")),
        Map.entry("horse_variant", Set.of("horse")), Map.entry("llama_variant", Set.of("llama", "trader_llama")),
        Map.entry("axolotl_variant", Set.of("axolotl")), Map.entry("sheep_color", Set.of("sheep")),
        Map.entry("shulker_color", Set.of("shulker")));

    static boolean applies(EntityCore core, String component) {
        Set<String> types = kTypes.get(component);
        return types != null && types.contains(core.type.getKey().getKey());
    }

    static Object read(EntityCore core, String component) {
        if (!applies(core, component)) {
            return null;
        }
        switch (component) {
            case "wolf_variant", "cat_variant", "frog_variant", "pig_variant", "cow_variant", "chicken_variant",
                "painting_variant", "zombie_nautilus_variant":
                return stored(core, "variant");
            case "wolf_sound_variant", "cat_sound_variant", "pig_sound_variant", "cow_sound_variant",
                "chicken_sound_variant":
                return stored(core, "sound_variant");
            case "wolf_collar", "cat_collar":
                return dye(stored(core, "CollarColor"));
            case "sheep_color", "shulker_color":
                return dye(stored(core, "Color"));
            case "salmon_size":
                return stored(core, "type");
            case "fox_variant", "mooshroom_variant":
                return stored(core, "Type");
            case "rabbit_variant":
                return indexed(stored(core, "RabbitType"), kRabbits, "brown");
            case "parrot_variant":
                return indexed(stored(core, "Variant"), kParrots, "red_blue");
            case "axolotl_variant":
                return indexed(stored(core, "Variant"), kAxolotls, "lucy");
            case "horse_variant":
                return indexed(stored(core, "Variant") instanceof Number number ? (Object) (number.intValue() & 255)
                    : null, kHorses, "white");
            case "llama_variant":
                return indexed(stored(core, "Variant"), kLlamas, "creamy");
            default:
                return null;
        }
    }
}
