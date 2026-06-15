package org.basaltmc.bukkit.item;

import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.ConfigurationSerializable;
import org.bukkit.configuration.serialization.ConfigurationSerialization;
import org.bukkit.configuration.serialization.SerializableAs;

/** Which meta class a material gets, and the text and serialized forms of a component map. */
public final class ItemMetas {
    public enum Kind {
        PLAIN, SKULL, LEATHER, ARMOR, BOOK, WRITABLE_BOOK, ENCHANTED_BOOK, FIREWORK, FIREWORK_EFFECT, POTION, BANNER,
        SHIELD, MAP, COMPASS, AXOLOTL, BUNDLE, STEW, TROPICAL, KNOWLEDGE, CROSSBOW, INSTRUMENT, OMINOUS, SPAWN_EGG
    }

    private static boolean registered;

    private ItemMetas() {}

    public static synchronized void register() {
        if (!registered) {
            registered = true;
            ConfigurationSerialization.registerClass(Serial.class);
        }
    }

    public static Kind kindOf(Material material) {
        String name = material.getKey().getKey();
        switch (name) {
            case "player_head", "zombie_head", "skeleton_skull", "wither_skeleton_skull", "creeper_head", "dragon_head",
                "piglin_head":
                return Kind.SKULL;
            case "leather_helmet", "leather_chestplate", "leather_leggings", "leather_boots", "leather_horse_armor",
                "wolf_armor":
                return Kind.LEATHER;
            case "written_book":
                return Kind.BOOK;
            case "writable_book":
                return Kind.WRITABLE_BOOK;
            case "enchanted_book":
                return Kind.ENCHANTED_BOOK;
            case "firework_rocket":
                return Kind.FIREWORK;
            case "firework_star":
                return Kind.FIREWORK_EFFECT;
            case "potion", "splash_potion", "lingering_potion", "tipped_arrow":
                return Kind.POTION;
            case "shield":
                return Kind.SHIELD;
            case "filled_map":
                return Kind.MAP;
            case "compass":
                return Kind.COMPASS;
            case "axolotl_bucket":
                return Kind.AXOLOTL;
            case "suspicious_stew":
                return Kind.STEW;
            case "tropical_fish_bucket":
                return Kind.TROPICAL;
            case "knowledge_book":
                return Kind.KNOWLEDGE;
            case "crossbow":
                return Kind.CROSSBOW;
            case "goat_horn":
                return Kind.INSTRUMENT;
            case "ominous_bottle":
                return Kind.OMINOUS;
            default:
                break;
        }
        if (name.endsWith("_banner")) {
            return Kind.BANNER;
        }
        if (name.endsWith("_bundle") || name.equals("bundle")) {
            return Kind.BUNDLE;
        }
        if (name.endsWith("_spawn_egg")) {
            return Kind.SPAWN_EGG;
        }
        if (name.endsWith("_helmet") || name.endsWith("_chestplate") || name.endsWith("_leggings")
            || name.endsWith("_boots")) {
            return Kind.ARMOR;
        }
        return Kind.PLAIN;
    }

    public static Class<? extends org.bukkit.inventory.meta.ItemMeta> metaClass(Material material) {
        switch (kindOf(material)) {
            case SKULL:
                return org.bukkit.inventory.meta.SkullMeta.class;
            case LEATHER:
                return org.bukkit.inventory.meta.LeatherArmorMeta.class;
            case ARMOR:
                return org.bukkit.inventory.meta.ArmorMeta.class;
            case BOOK:
                return org.bukkit.inventory.meta.BookMeta.class;
            case WRITABLE_BOOK:
                return org.bukkit.inventory.meta.WritableBookMeta.class;
            case ENCHANTED_BOOK:
                return org.bukkit.inventory.meta.EnchantmentStorageMeta.class;
            case FIREWORK:
                return org.bukkit.inventory.meta.FireworkMeta.class;
            case FIREWORK_EFFECT:
                return org.bukkit.inventory.meta.FireworkEffectMeta.class;
            case POTION:
                return org.bukkit.inventory.meta.PotionMeta.class;
            case BANNER:
                return org.bukkit.inventory.meta.BannerMeta.class;
            case SHIELD:
                return org.bukkit.inventory.meta.ShieldMeta.class;
            case MAP:
                return org.bukkit.inventory.meta.MapMeta.class;
            case COMPASS:
                return org.bukkit.inventory.meta.CompassMeta.class;
            case AXOLOTL:
                return org.bukkit.inventory.meta.AxolotlBucketMeta.class;
            case BUNDLE:
                return org.bukkit.inventory.meta.BundleMeta.class;
            case STEW:
                return org.bukkit.inventory.meta.SuspiciousStewMeta.class;
            case TROPICAL:
                return org.bukkit.inventory.meta.TropicalFishBucketMeta.class;
            case KNOWLEDGE:
                return org.bukkit.inventory.meta.KnowledgeBookMeta.class;
            case CROSSBOW:
                return org.bukkit.inventory.meta.CrossbowMeta.class;
            case INSTRUMENT:
                return org.bukkit.inventory.meta.MusicInstrumentMeta.class;
            case OMINOUS:
                return org.bukkit.inventory.meta.OminousBottleMeta.class;
            case SPAWN_EGG:
                return org.bukkit.inventory.meta.SpawnEggMeta.class;
            default:
                return org.bukkit.inventory.meta.ItemMeta.class;
        }
    }

    public static MetaItem create(Material material, Map<String, Object> components) {
        switch (kindOf(material)) {
            case SKULL:
                return new MetaSkull(material, components);
            case LEATHER:
                return new MetaLeatherArmor(material, components);
            case ARMOR:
                return new MetaArmor(material, components);
            case BOOK:
                return new MetaBook(material, components);
            case WRITABLE_BOOK:
                return new MetaWritableBook(material, components);
            case ENCHANTED_BOOK:
                return new MetaEnchantedBook(material, components);
            case FIREWORK:
                return new MetaFirework(material, components);
            case FIREWORK_EFFECT:
                return new MetaFireworkEffect(material, components);
            case POTION:
                return new MetaPotion(material, components);
            case BANNER:
                return new MetaBanner(material, components);
            case SHIELD:
                return new MetaShield(material, components);
            case MAP:
                return new MetaMap(material, components);
            case COMPASS:
                return new MetaCompass(material, components);
            case AXOLOTL:
                return new MetaVariants.Axolotls(material, components);
            case BUNDLE:
                return new MetaContainers.Bundles(material, components);
            case STEW:
                return new MetaContainers.Stews(material, components);
            case TROPICAL:
                return new MetaVariants.TropicalFishes(material, components);
            case KNOWLEDGE:
                return new MetaVariants.Knowledge(material, components);
            case CROSSBOW:
                return new MetaContainers.Crossbows(material, components);
            case INSTRUMENT:
                return new MetaVariants.Instruments(material, components);
            case OMINOUS:
                return new MetaVariants.Ominous(material, components);
            case SPAWN_EGG:
                return new MetaSpawnEgg(material, components);
            default:
                return new MetaItem(material, components);
        }
    }

    public static String snbt(Map<String, Object> components) {
        StringBuilder out = new StringBuilder("[");
        boolean first = true;
        for (Map.Entry<String, Object> entry : components.entrySet()) {
            if (!first) {
                out.append(',');
            }
            first = false;
            String name = entry.getKey().startsWith("minecraft:") ? entry.getKey().substring(10) : entry.getKey();
            out.append(name).append('=');
            write(out, entry.getValue());
        }
        return out.append(']').toString();
    }

    public static void write(StringBuilder out, Object value) {
        if (value instanceof Map<?, ?> map) {
            out.append('{');
            boolean first = true;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                if (!first) {
                    out.append(',');
                }
                first = false;
                out.append(quoteKey((String) entry.getKey())).append(':');
                write(out, entry.getValue());
            }
            out.append('}');
        } else if (value instanceof List<?> list) {
            out.append('[');
            for (int at = 0; at < list.size(); at++) {
                if (at > 0) {
                    out.append(',');
                }
                write(out, list.get(at));
            }
            out.append(']');
        } else if (value instanceof String text) {
            out.append('\'').append(text.replace("\\", "\\\\").replace("'", "\\'")).append('\'');
        } else if (value instanceof Byte number) {
            out.append(number).append('b');
        } else if (value instanceof Short number) {
            out.append(number).append('s');
        } else if (value instanceof Long number) {
            out.append(number).append('L');
        } else if (value instanceof Float number) {
            out.append(number).append('f');
        } else if (value instanceof Double number) {
            out.append(number).append('d');
        } else if (value instanceof int[] ints) {
            out.append("[I;");
            for (int at = 0; at < ints.length; at++) {
                out.append(at > 0 ? "," : "").append(ints[at]);
            }
            out.append(']');
        } else if (value instanceof long[] longs) {
            out.append("[L;");
            for (int at = 0; at < longs.length; at++) {
                out.append(at > 0 ? "," : "").append(longs[at]).append('L');
            }
            out.append(']');
        } else if (value instanceof byte[] bytes) {
            out.append("[B;");
            for (int at = 0; at < bytes.length; at++) {
                out.append(at > 0 ? "," : "").append(bytes[at]).append('b');
            }
            out.append(']');
        } else {
            out.append(value);
        }
    }

    private static String quoteKey(String key) {
        for (int at = 0; at < key.length(); at++) {
            char ch = key.charAt(at);
            if (!(Character.isLetterOrDigit(ch) || ch == '_' || ch == '-' || ch == '.' || ch == '+')) {
                return "'" + key.replace("\\", "\\\\").replace("'", "\\'") + "'";
            }
        }
        return key;
    }

    public static Map<String, Object> serialize(MetaItem meta) {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put("meta-type", "TYPE:" + meta.materialType().getKey());
        out.put("components", Base64.getEncoder().encodeToString(Nbt.write(meta.components())));
        return out;
    }

    @SuppressWarnings("unchecked")
    public static MetaItem deserialize(Material material, Map<String, Object> args) {
        Object encoded = args.get("components");
        if (encoded instanceof String text) {
            Map<String, Object> components = Nbt.read(Base64.getDecoder().decode(text));
            return create(material, components == null ? Nbt.compound() : components);
        }
        MetaItem meta = create(material, Nbt.compound());
        Object name = args.get("display-name");
        if (name instanceof String text) {
            meta.setDisplayName(text);
        }
        if (args.get("lore") instanceof List<?> lore) {
            meta.setLore((List<String>) lore);
        }
        if (args.get("enchants") instanceof Map<?, ?> enchants) {
            for (Map.Entry<?, ?> entry : enchants.entrySet()) {
                org.bukkit.NamespacedKey key = org.bukkit.NamespacedKey.fromString(String.valueOf(entry.getKey()).toLowerCase(Locale.ROOT));
                org.bukkit.enchantments.Enchantment enchantment = key == null ? null : org.bukkit.enchantments.Enchantment.getByKey(key);
                if (enchantment != null && entry.getValue() instanceof Number level) {
                    meta.addEnchant(enchantment, level.intValue(), true);
                }
            }
        }
        if (args.get("Unbreakable") instanceof Boolean flag) {
            meta.setUnbreakable(flag);
        }
        if (args.get("Damage") instanceof Number damage) {
            meta.setDamage(damage.intValue());
        }
        if (args.get("custom-model-data") instanceof Number model) {
            meta.setCustomModelData(model.intValue());
        }
        if (args.get("repair-cost") instanceof Number cost) {
            meta.setRepairCost(cost.intValue());
        }
        if (args.get("ItemFlags") instanceof List<?> flags) {
            for (Object flag : flags) {
                try {
                    meta.addItemFlags(org.bukkit.inventory.ItemFlag.valueOf(String.valueOf(flag)));
                } catch (IllegalArgumentException ignored) {
                    continue;
                }
            }
        }
        return meta;
    }

    @SerializableAs("ItemMeta")
    public static final class Serial implements ConfigurationSerializable {
        private final Map<String, Object> args;

        private Serial(Map<String, Object> args) {
            this.args = args;
        }

        @Override
        public Map<String, Object> serialize() {
            return args;
        }

        public static MetaItem deserialize(Map<String, Object> args) {
            Material material = Material.AIR;
            if (args.get("meta-type") instanceof String kind && kind.startsWith("TYPE:")) {
                Material parsed = Material.matchMaterial(kind.substring(5));
                material = parsed == null ? Material.AIR : parsed;
            }
            return ItemMetas.deserialize(material, args);
        }

        public static MetaItem valueOf(Map<String, Object> args) {
            return deserialize(args);
        }
    }
}
