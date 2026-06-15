package org.basaltmc.bukkit.item;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.gson.GsonComponentSerializer;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;

/** Text components as they sit inside item data: NBT, with strings, legacy codes and Adventure on either side. */
public final class Texts {
    private static final GsonComponentSerializer GSON = GsonComponentSerializer.gson();
    private static final LegacyComponentSerializer LEGACY = LegacyComponentSerializer.legacySection();

    private Texts() {}

    public static Object toNbt(Component component) {
        return fromJson(GSON.serializeToTree(component));
    }

    public static Component fromNbt(Object value) {
        if (value == null) {
            return null;
        }
        try {
            return GSON.deserializeFromTree(toJson(value));
        } catch (RuntimeException e) {
            return Component.text(String.valueOf(value));
        }
    }

    public static Object legacyToNbt(String legacy) {
        return toNbt(LEGACY.deserialize(legacy));
    }

    public static String legacyFromNbt(Object value) {
        Component component = fromNbt(value);
        return component == null ? null : LEGACY.serialize(component);
    }

    public static Component fromLegacy(String legacy) {
        return legacy == null ? null : LEGACY.deserialize(legacy);
    }

    public static String toLegacy(Component component) {
        return component == null ? null : LEGACY.serialize(component);
    }

    public static String json(Component component) {
        return GSON.serialize(component);
    }

    public static Component fromJson(String json) {
        return GSON.deserialize(json);
    }

    private static Object fromJson(JsonElement element) {
        if (element == null || element instanceof JsonNull) {
            return "";
        }
        if (element instanceof JsonPrimitive primitive) {
            if (primitive.isBoolean()) {
                return Nbt.bool(primitive.getAsBoolean());
            }
            if (primitive.isNumber()) {
                double number = primitive.getAsDouble();
                return number == Math.rint(number) && Math.abs(number) < Integer.MAX_VALUE
                    ? (Object) (int) number : (Object) number;
            }
            return primitive.getAsString();
        }
        if (element instanceof JsonArray array) {
            List<Object> converted = new ArrayList<>(array.size());
            boolean uniform = true;
            Class<?> kind = null;
            for (JsonElement child : array) {
                Object value = fromJson(child);
                converted.add(value);
                Class<?> own = value instanceof Map ? Map.class : value.getClass();
                if (kind == null) {
                    kind = own;
                } else if (kind != own) {
                    uniform = false;
                }
            }
            if (!uniform) {
                for (int at = 0; at < converted.size(); at++) {
                    if (!(converted.get(at) instanceof Map)) {
                        Map<String, Object> wrapped = new LinkedHashMap<>();
                        wrapped.put("", converted.get(at));
                        converted.set(at, wrapped);
                    }
                }
            }
            return converted;
        }
        JsonObject object = element.getAsJsonObject();
        Map<String, Object> map = new LinkedHashMap<>();
        for (Map.Entry<String, JsonElement> entry : object.entrySet()) {
            map.put(entry.getKey(), fromJson(entry.getValue()));
        }
        return map;
    }

    private static JsonElement toJson(Object value) {
        if (value instanceof String text) {
            return new JsonPrimitive(text);
        }
        if (value instanceof Number number) {
            return new JsonPrimitive(number);
        }
        if (value instanceof Boolean flag) {
            return new JsonPrimitive(flag);
        }
        if (value instanceof List<?> list) {
            JsonArray array = new JsonArray();
            for (Object child : list) {
                array.add(toJson(child));
            }
            return array;
        }
        if (value instanceof Map<?, ?> map) {
            if (map.size() == 1 && map.containsKey("")) {
                return toJson(map.get(""));
            }
            JsonObject object = new JsonObject();
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                object.add((String) entry.getKey(), isFlagKey((String) entry.getKey(), entry.getValue())
                    ? new JsonPrimitive(((Number) entry.getValue()).intValue() != 0) : toJson(entry.getValue()));
            }
            return object;
        }
        return new JsonPrimitive(String.valueOf(value));
    }

    private static boolean isFlagKey(String key, Object value) {
        if (!(value instanceof Byte)) {
            return false;
        }
        switch (key) {
            case "bold", "italic", "underlined", "strikethrough", "obfuscated", "shadow", "interpret", "plain":
                return true;
            default:
                return false;
        }
    }
}
