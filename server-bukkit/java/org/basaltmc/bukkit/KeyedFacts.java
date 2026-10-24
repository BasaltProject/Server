package org.basaltmc.bukkit;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.Sound;
import org.bukkit.attribute.Attribute;
import org.bukkit.damage.DamageScaling;
import org.bukkit.damage.DamageType;
import org.bukkit.damage.DeathMessageType;

/** What a registry entry known only by its key can still tell, read from the server's registry data. */
final class KeyedFacts {
    static final Object kNone = new Object();

    private static volatile Map<String, double[]> attributes;
    private static volatile Map<String, String> sentiments;

    private KeyedFacts() {}

    private static Map<String, Object> data(String registry, NamespacedKey key) {
        byte[] bytes = LeftoverNatives.registryData(registry, key.toString());
        return bytes == null ? null : Nbt.read(bytes);
    }

    private static synchronized void loadAttributes() {
        if (attributes != null) {
            return;
        }
        Map<String, double[]> numbers = new HashMap<>();
        Map<String, String> feelings = new HashMap<>();
        try (java.io.InputStream in = KeyedFacts.class.getResourceAsStream("/attributes.txt")) {
            if (in != null) {
                for (String line : new String(in.readAllBytes(), java.nio.charset.StandardCharsets.UTF_8).split("\n")) {
                    String[] part = line.strip().split(" ");
                    if (part.length == 5) {
                        numbers.put(part[0], new double[] {Double.parseDouble(part[1]), Double.parseDouble(part[2]),
                            Double.parseDouble(part[3])});
                        feelings.put(part[0], part[4]);
                    }
                }
            }
        } catch (java.io.IOException failure) {
            Natives.log(2, "Bukkit", "Could not read the attribute list: " + failure);
        }
        sentiments = feelings;
        attributes = numbers;
    }

    static double attributeDefault(String name) {
        loadAttributes();
        double[] found = attributes.get(name);
        return found == null ? 0.0 : found[0];
    }

    static double[] attributeRange(String name) {
        loadAttributes();
        return attributes.get(name);
    }

    static Attribute.Sentiment sentiment(String name) {
        loadAttributes();
        return Attribute.Sentiment.valueOf(sentiments.getOrDefault(name, "positive").toUpperCase(Locale.ROOT));
    }

    private static <E extends Enum<E>> E constant(Class<E> type, Object stored, E fallback) {
        try {
            return stored == null ? fallback : Enum.valueOf(type, String.valueOf(stored).toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException unknown) {
            return fallback;
        }
    }

    private static Sound sound(Object stored) {
        if (stored instanceof Map<?, ?> inline) {
            stored = inline.get("sound_id");
        }
        return stored == null ? null : Registry.SOUNDS.get(NamespacedKey.fromString(String.valueOf(stored)));
    }

    static Object answer(Class<?> type, NamespacedKey key, Method method) {
        String name = method.getName();
        if (type == Attribute.class) {
            switch (name) {
                case "getDefaultValue":
                    return attributeDefault(key.getKey());
                case "getSentiment":
                    return sentiment(key.getKey());
                case "getTranslationKey", "translationKey":
                    return "attribute.name." + key.getKey();
                default:
                    return kNone;
            }
        }
        if (type == DamageType.class) {
            Map<String, Object> data = data("damage_type", key);
            switch (name) {
                case "getTranslationKey":
                    return "death.attack." + (data == null ? key.getKey() : Nbt.string(data, "message_id", key.getKey()));
                case "getDamageScaling":
                    return constant(DamageScaling.class, data == null ? null : data.get("scaling"),
                        DamageScaling.WHEN_CAUSED_BY_LIVING_NON_PLAYER);
                case "getDamageEffect":
                    return BasaltApiBridge.damageEffect(data == null ? "hurt" : Nbt.string(data, "effects", "hurt"));
                case "getDeathMessageType":
                    return constant(DeathMessageType.class, data == null ? null : data.get("death_message_type"),
                        DeathMessageType.DEFAULT);
                case "getExhaustion":
                    return (float) Nbt.real(data, "exhaustion", 0.0);
                default:
                    return kNone;
            }
        }
        if (type == org.bukkit.JukeboxSong.class) {
            Map<String, Object> data = data("jukebox_song", key);
            switch (name) {
                case "getTranslationKey":
                    return "jukebox_song." + key.getNamespace() + "." + key.getKey();
                case "getSound":
                    return data == null ? null : sound(data.get("sound_event"));
                case "getDescription":
                    return data == null ? Component.translatable("jukebox_song." + key.getNamespace() + "."
                        + key.getKey()) : Texts.fromNbt(data.get("description"));
                case "getLengthInSeconds":
                    return (float) Nbt.real(data, "length_in_seconds", 0.0);
                case "getComparatorOutput":
                    return Nbt.integer(data, "comparator_output", 0);
                default:
                    return kNone;
            }
        }
        return kNone;
    }
}
