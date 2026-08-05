package org.basaltmc.bukkit.item;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.BasaltItemStack;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

public final class ItemJson {
    private ItemJson() {}

    public static JsonObject write(ItemStack stack, int dataVersion) {
        JsonObject out = new JsonObject();
        out.addProperty("id", stack.getType().getKey().toString());
        out.addProperty("count", stack.getAmount());
        Map<String, Object> components = org.basaltmc.bukkit.Items.componentsOf(stack);
        if (components != null && !components.isEmpty()) {
            out.add("components", element(components));
        }
        out.addProperty("DataVersion", dataVersion);
        return out;
    }

    public static ItemStack read(JsonObject json) {
        Material material = Material.matchMaterial(json.get("id").getAsString());
        if (material == null || material.isAir()) {
            throw new IllegalArgumentException("Unknown item " + json.get("id"));
        }
        int count = json.has("count") ? json.get("count").getAsInt() : 1;
        Object components = json.has("components") ? value(json.get("components")) : Nbt.compound();
        @SuppressWarnings("unchecked")
        Map<String, Object> map = components instanceof Map<?, ?> found ? (Map<String, Object>) found : Nbt.compound();
        return new BasaltItemStack(material, count, map);
    }

    private static JsonElement element(Object value) {
        if (value instanceof Map<?, ?> map) {
            JsonObject out = new JsonObject();
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                out.add((String) entry.getKey(), element(entry.getValue()));
            }
            return out;
        }
        if (value instanceof List<?> list) {
            JsonArray out = new JsonArray();
            for (Object entry : list) {
                out.add(element(entry));
            }
            return out;
        }
        if (value instanceof byte[] bytes) {
            JsonArray out = new JsonArray();
            for (byte one : bytes) {
                out.add(one);
            }
            return out;
        }
        if (value instanceof int[] ints) {
            JsonArray out = new JsonArray();
            for (int one : ints) {
                out.add(one);
            }
            return out;
        }
        if (value instanceof long[] longs) {
            JsonArray out = new JsonArray();
            for (long one : longs) {
                out.add(one);
            }
            return out;
        }
        if (value instanceof Number number) {
            return new JsonPrimitive(number);
        }
        if (value instanceof Boolean flag) {
            return new JsonPrimitive(flag);
        }
        return new JsonPrimitive(String.valueOf(value));
    }

    private static Object value(JsonElement element) {
        if (element.isJsonObject()) {
            Map<String, Object> out = Nbt.compound();
            for (Map.Entry<String, JsonElement> entry : element.getAsJsonObject().entrySet()) {
                out.put(entry.getKey(), value(entry.getValue()));
            }
            return out;
        }
        if (element.isJsonArray()) {
            List<Object> out = new java.util.ArrayList<>();
            for (JsonElement entry : element.getAsJsonArray()) {
                out.add(value(entry));
            }
            return out;
        }
        JsonPrimitive primitive = element.getAsJsonPrimitive();
        if (primitive.isBoolean()) {
            return (byte) (primitive.getAsBoolean() ? 1 : 0);
        }
        if (primitive.isNumber()) {
            String text = primitive.getAsString();
            if (text.contains(".") || text.contains("e") || text.contains("E")) {
                return primitive.getAsDouble();
            }
            long whole = primitive.getAsLong();
            return whole >= Integer.MIN_VALUE && whole <= Integer.MAX_VALUE ? (Object) (int) whole : (Object) whole;
        }
        return primitive.getAsString();
    }
}
