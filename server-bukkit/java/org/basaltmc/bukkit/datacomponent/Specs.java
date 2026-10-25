package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.registry.RegistryAccess;
import io.papermc.paper.registry.RegistryKey;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import net.kyori.adventure.key.Key;
import org.basaltmc.bukkit.datacomponent.Dyn.Spec;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.FireworkEffect;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;

final class Specs {
    private static final Map<String, Spec> kSpecs = Map.ofEntries(Map.entry("tool", ItemSpecs.TOOL),
        Map.entry("equippable", ItemSpecs.EQUIPPABLE), Map.entry("consumable", ItemSpecs.CONSUMABLE),
        Map.entry("death_protection", ItemSpecs.DEATH_PROTECTION),
        Map.entry("attribute_modifiers", ItemSpecs.MODIFIERS), Map.entry("weapon", ItemSpecs.WEAPON),
        Map.entry("attack_range", ItemSpecs.ATTACK_RANGE), Map.entry("use_effects", ItemSpecs.USE_EFFECTS),
        Map.entry("piercing_weapon", ItemSpecs.PIERCING), Map.entry("kinetic_weapon", ItemSpecs.KINETIC),
        Map.entry("attack_animation", ItemSpecs.SWING), Map.entry("interact_animation", ItemSpecs.SWING),
        Map.entry("blocks_attacks", ItemSpecs.BLOCKS_ATTACKS), Map.entry("potion_contents", ContentSpecs.POTION),
        Map.entry("bundle_contents", ContentSpecs.BUNDLE), Map.entry("charged_projectiles", ContentSpecs.CHARGED),
        Map.entry("container", ContentSpecs.CONTAINER), Map.entry("pot_decorations", ContentSpecs.DECORATIONS),
        Map.entry("suspicious_stew_effects", ContentSpecs.STEW),
        Map.entry("map_decorations", ContentSpecs.MAP_DECORATIONS), Map.entry("container_loot", ContentSpecs.LOOT),
        Map.entry("written_book_content", ContentSpecs.WRITTEN),
        Map.entry("writable_book_content", ContentSpecs.WRITABLE), Map.entry("trim", ContentSpecs.TRIM),
        Map.entry("lodestone_tracker", ContentSpecs.LODESTONE), Map.entry("fireworks", ContentSpecs.FIREWORKS),
        Map.entry("banner_patterns", ContentSpecs.LAYERS), Map.entry("block_state", ContentSpecs.BLOCK_DATA),
        Map.entry("jukebox_playable", ContentSpecs.JUKEBOX), Map.entry("can_place_on", ContentSpecs.PREDICATE),
        Map.entry("can_break", ContentSpecs.PREDICATE), Map.entry("sulfur_cube_content", ContentSpecs.SULFUR),
        Map.entry("mob_visibility", ContentSpecs.VISIBILITY), Map.entry("villager_food", ContentSpecs.FOOD),
        Map.entry("sign_text_front", ContentSpecs.SIGN), Map.entry("sign_text_back", ContentSpecs.SIGN),
        Map.entry("profile", ContentSpecs.PROFILE));

    private static final Map<String, Class<?>> kVariants = Map.ofEntries(
        Map.entry("villager_variant", org.bukkit.entity.Villager.Type.class),
        Map.entry("wolf_variant", org.bukkit.entity.Wolf.Variant.class),
        Map.entry("wolf_sound_variant", org.bukkit.entity.Wolf.SoundVariant.class),
        Map.entry("fox_variant", org.bukkit.entity.Fox.Type.class),
        Map.entry("salmon_size", org.bukkit.entity.Salmon.Variant.class),
        Map.entry("parrot_variant", org.bukkit.entity.Parrot.Variant.class),
        Map.entry("tropical_fish_pattern", org.bukkit.entity.TropicalFish.Pattern.class),
        Map.entry("mooshroom_variant", org.bukkit.entity.MushroomCow.Variant.class),
        Map.entry("rabbit_variant", org.bukkit.entity.Rabbit.Type.class),
        Map.entry("pig_variant", org.bukkit.entity.Pig.Variant.class),
        Map.entry("pig_sound_variant", org.bukkit.entity.Pig.SoundVariant.class),
        Map.entry("cow_variant", org.bukkit.entity.Cow.Variant.class),
        Map.entry("cow_sound_variant", org.bukkit.entity.Cow.SoundVariant.class),
        Map.entry("chicken_variant", org.bukkit.entity.Chicken.Variant.class),
        Map.entry("chicken_sound_variant", org.bukkit.entity.Chicken.SoundVariant.class),
        Map.entry("frog_variant", org.bukkit.entity.Frog.Variant.class),
        Map.entry("horse_variant", org.bukkit.entity.Horse.Color.class),
        Map.entry("painting_variant", org.bukkit.Art.class),
        Map.entry("llama_variant", org.bukkit.entity.Llama.Color.class),
        Map.entry("axolotl_variant", org.bukkit.entity.Axolotl.Variant.class),
        Map.entry("zombie_nautilus_variant", org.bukkit.entity.ZombieNautilus.Variant.class),
        Map.entry("cat_variant", org.bukkit.entity.Cat.Type.class),
        Map.entry("cat_sound_variant", org.bukkit.entity.Cat.SoundVariant.class),
        Map.entry("instrument", org.bukkit.MusicInstrument.class),
        Map.entry("provides_trim_material", org.bukkit.inventory.meta.trim.TrimMaterial.class),
        Map.entry("damage_type", org.bukkit.damage.DamageType.class),
        Map.entry("provides_pottery_pattern", io.papermc.paper.block.pot.PotPatternType.class),
        Map.entry("map_post_processing", io.papermc.paper.item.MapPostProcessing.class));

    private Specs() {}

    static boolean has(String name) {
        return kSpecs.containsKey(name) || kVariants.containsKey(name) || isPlain(name);
    }

    private static boolean isPlain(String name) {
        return switch (name) {
            case "minimum_attack_charge", "potion_duration_scale", "recipes", "firework_explosion", "break_sound",
                "provides_banner_patterns", "cat_collar", "wolf_collar", "sheep_color", "shulker_color",
                "cushion_color", "tropical_fish_base_color", "tropical_fish_pattern_color" -> true;
            default -> false;
        };
    }

    static Spec spec(String name) {
        return kSpecs.get(name);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    static Object decode(String name, Object nbt) {
        Spec spec = kSpecs.get(name);
        if (spec != null) {
            return spec.read(nbt);
        }
        Class<?> variant = kVariants.get(name);
        if (variant != null) {
            String id = String.valueOf(nbt);
            if (variant.isEnum()) {
                String bare = id.substring(id.indexOf(':') + 1).toUpperCase(Locale.ROOT);
                return Enum.valueOf((Class<Enum>) variant, bare);
            }
            return RegistryAccess.registryAccess().getRegistry((Class<Keyed>) variant).get(NamespacedKey.fromString(id));
        }
        switch (name) {
            case "minimum_attack_charge", "potion_duration_scale":
                return nbt instanceof Number number ? number.floatValue() : 0F;
            case "recipes": {
                List<Key> keys = new ArrayList<>();
                List<Object> list = Nbt.list(nbt);
                for (Object id : list == null ? List.of() : list) {
                    keys.add(Key.key(String.valueOf(id)));
                }
                return keys;
            }
            case "firework_explosion":
                return Convert.firework(Nbt.map(nbt));
            case "break_sound":
                return Key.key(String.valueOf(nbt));
            case "provides_banner_patterns":
                return Wrap.set(RegistryKey.BANNER_PATTERN, nbt);
            default:
                return Convert.dye(String.valueOf(nbt));
        }
    }

    static Object encode(String name, Object value) {
        if (value instanceof FireworkEffect effect) {
            return Convert.firework(effect);
        }
        if (value instanceof Float || value instanceof Double) {
            return ((Number) value).floatValue();
        }
        if (value instanceof io.papermc.paper.registry.set.RegistryKeySet<?> set) {
            return Wrap.setNbt(set);
        }
        if (name.equals("recipes") && value instanceof List<?> keys) {
            List<Object> out = new ArrayList<>();
            for (Object key : keys) {
                out.add(((Key) key).asString());
            }
            return out;
        }
        if (value instanceof Enum<?> constant && !(value instanceof Keyed)) {
            return constant.name().toLowerCase(Locale.ROOT);
        }
        if (value instanceof Keyed keyed) {
            return keyed.getKey().toString();
        }
        return null;
    }
}
