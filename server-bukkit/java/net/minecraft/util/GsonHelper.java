// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.google.gson.Gson;
import com.google.gson.JsonParseException;
import com.google.gson.JsonPrimitive;
import com.google.gson.JsonSyntaxException;
import com.google.gson.stream.JsonReader;
import java.util.Map;
import org.basaltmc.bukkit.Unsupported;
import com.google.gson.JsonArray;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.reflect.TypeToken;
import com.google.gson.stream.JsonWriter;
import java.io.IOException;
import java.io.Reader;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.Comparator;
import net.minecraft.core.Holder;
import net.minecraft.world.item.Item;
public class GsonHelper {
    private static final Gson GSON = new com.google.gson.GsonBuilder().create();
    protected GsonHelper() {
    }
    public static Object fromJson(Gson gson, String string, Class type) {
        return gson.fromJson(string, type);
    }
    public static boolean isStringValue(JsonObject node, String name) {
        return isValidPrimitive(node, name) && node.getAsJsonPrimitive(name).isString();
    }
    public static boolean isStringValue(JsonElement node) {
        return node.isJsonPrimitive() && node.getAsJsonPrimitive().isString();
    }
    public static boolean isNumberValue(JsonObject node, String name) {
        return isValidPrimitive(node, name) && node.getAsJsonPrimitive(name).isNumber();
    }
    public static boolean isNumberValue(JsonElement node) {
        return node.isJsonPrimitive() && node.getAsJsonPrimitive().isNumber();
    }
    public static boolean isBooleanValue(JsonObject node, String name) {
        return isValidPrimitive(node, name) && node.getAsJsonPrimitive(name).isBoolean();
    }
    public static boolean isBooleanValue(JsonElement node) {
        return node.isJsonPrimitive() && node.getAsJsonPrimitive().isBoolean();
    }
    public static boolean isArrayNode(JsonObject node, String name) {
        return isValidNode(node, name) && node.get(name).isJsonArray();
    }
    public static boolean isObjectNode(JsonObject node, String name) {
        return isValidNode(node, name) && node.get(name).isJsonObject();
    }
    public static boolean isValidPrimitive(JsonObject node, String name) {
        return isValidNode(node, name) && node.get(name).isJsonPrimitive();
    }
    public static boolean isValidNode(JsonObject node, String name) {
        return node != null && node.get(name) != null && !node.get(name).isJsonNull();
    }
    public static JsonElement getNonNull(JsonObject object, String name) {
        JsonElement element = object.get(name);
        if (element == null || element.isJsonNull()) {
            throw new JsonSyntaxException("Missing field " + name);
        }
        return element;
    }
    public static String convertToString(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isString()) {
            return element.getAsString();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a string, was " + getType(element));
    }
    public static String getAsString(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToString(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a string");
    }
    public static String getAsString(JsonObject object, String name, String def) {
        return object.has(name) ? convertToString(object.get(name), name) : def;
    }
    public static Holder<Item> convertToItem(JsonElement element, String name) {
        if (element.isJsonPrimitive()) {
            String id = element.getAsString();
            return net.minecraft.core.registries.BuiltInRegistries.ITEM.get(net.minecraft.resources.Identifier.parse(id))
                .map(found -> (Holder<Item>) found)
                .orElseThrow(() -> new com.google.gson.JsonSyntaxException("Expected " + name + " to be an item, was unknown string '" + id + "'"));
        }
        throw new com.google.gson.JsonSyntaxException("Expected " + name + " to be an item, was " + getType(element));
    }
    public static Holder<Item> getAsItem(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToItem(object.get(name), name);
        }
        throw new com.google.gson.JsonSyntaxException("Missing " + name + ", expected to find an item");
    }
    public static Holder<Item> getAsItem(JsonObject object, String name, Holder<Item> def) {
        return object.has(name) ? convertToItem(object.get(name), name) : def;
    }
    public static boolean convertToBoolean(JsonElement element, String name) {
        if (element.isJsonPrimitive() && true) {
            return element.getAsBoolean();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Boolean, was " + getType(element));
    }
    public static boolean getAsBoolean(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToBoolean(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Boolean");
    }
    public static boolean getAsBoolean(JsonObject object, String name, boolean def) {
        return object.has(name) ? convertToBoolean(object.get(name), name) : def;
    }
    public static double convertToDouble(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsDouble();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Double, was " + getType(element));
    }
    public static double getAsDouble(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToDouble(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Double");
    }
    public static double getAsDouble(JsonObject object, String name, double def) {
        return object.has(name) ? convertToDouble(object.get(name), name) : def;
    }
    public static float convertToFloat(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsFloat();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Float, was " + getType(element));
    }
    public static float getAsFloat(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToFloat(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Float");
    }
    public static float getAsFloat(JsonObject object, String name, float def) {
        return object.has(name) ? convertToFloat(object.get(name), name) : def;
    }
    public static long convertToLong(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsLong();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Long, was " + getType(element));
    }
    public static long getAsLong(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToLong(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Long");
    }
    public static long getAsLong(JsonObject object, String name, long def) {
        return object.has(name) ? convertToLong(object.get(name), name) : def;
    }
    public static int convertToInt(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsInt();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Int, was " + getType(element));
    }
    public static int getAsInt(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToInt(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Int");
    }
    public static int getAsInt(JsonObject object, String name, int def) {
        return object.has(name) ? convertToInt(object.get(name), name) : def;
    }
    public static byte convertToByte(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsByte();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Byte, was " + getType(element));
    }
    public static byte getAsByte(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToByte(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Byte");
    }
    public static byte getAsByte(JsonObject object, String name, byte def) {
        return object.has(name) ? convertToByte(object.get(name), name) : def;
    }
    public static char convertToCharacter(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsCharacter();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Character, was " + getType(element));
    }
    public static char getAsCharacter(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToCharacter(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Character");
    }
    public static char getAsCharacter(JsonObject object, String name, char def) {
        return object.has(name) ? convertToCharacter(object.get(name), name) : def;
    }
    public static BigDecimal convertToBigDecimal(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsBigDecimal();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a BigDecimal, was " + getType(element));
    }
    public static BigDecimal getAsBigDecimal(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToBigDecimal(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a BigDecimal");
    }
    public static BigDecimal getAsBigDecimal(JsonObject object, String name, BigDecimal def) {
        return object.has(name) ? convertToBigDecimal(object.get(name), name) : def;
    }
    public static BigInteger convertToBigInteger(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsBigInteger();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a BigInteger, was " + getType(element));
    }
    public static BigInteger getAsBigInteger(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToBigInteger(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a BigInteger");
    }
    public static BigInteger getAsBigInteger(JsonObject object, String name, BigInteger def) {
        return object.has(name) ? convertToBigInteger(object.get(name), name) : def;
    }
    public static short convertToShort(JsonElement element, String name) {
        if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isNumber()) {
            return element.getAsShort();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a Short, was " + getType(element));
    }
    public static short getAsShort(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToShort(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a Short");
    }
    public static short getAsShort(JsonObject object, String name, short def) {
        return object.has(name) ? convertToShort(object.get(name), name) : def;
    }
    public static JsonObject convertToJsonObject(JsonElement element, String name) {
        if (element.isJsonObject()) {
            return element.getAsJsonObject();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a JsonObject, was " + getType(element));
    }
    public static JsonObject getAsJsonObject(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToJsonObject(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a JsonObject");
    }
    public static JsonObject getAsJsonObject(JsonObject object, String name, JsonObject def) {
        return object.has(name) ? convertToJsonObject(object.get(name), name) : def;
    }
    public static JsonArray convertToJsonArray(JsonElement element, String name) {
        if (element.isJsonArray()) {
            return element.getAsJsonArray();
        }
        throw new JsonSyntaxException("Expected " + name + " to be a JsonArray, was " + getType(element));
    }
    public static JsonArray getAsJsonArray(JsonObject object, String name) {
        if (object.has(name)) {
            return convertToJsonArray(object.get(name), name);
        }
        throw new JsonSyntaxException("Missing " + name + ", expected to find a JsonArray");
    }
    public static JsonArray getAsJsonArray(JsonObject object, String name, JsonArray def) {
        return object.has(name) ? convertToJsonArray(object.get(name), name) : def;
    }
    public static <T> T convertToObject(JsonElement element, String name, JsonDeserializationContext context, Class<? extends T> clazz) {
        return context.deserialize(element, clazz);
    }
    public static <T> T getAsObject(JsonObject object, String name, JsonDeserializationContext context, Class<? extends T> clazz) {
        if (object.has(name)) {
            return convertToObject(object.get(name), name, context, clazz);
        }
        throw new JsonSyntaxException("Missing " + name);
    }
    public static <T> T getAsObject(JsonObject object, String name, T def, JsonDeserializationContext context, Class<? extends T> clazz) {
        return object.has(name) ? convertToObject(object.get(name), name, context, clazz) : def;
    }
    public static String getType(JsonElement element) {
        String text = String.valueOf(element);
        if (text.length() > 10) {
            text = text.substring(0, 10) + "...";
        }
        if (element == null) {
            return "null (missing)";
        }
        if (element.isJsonNull()) {
            return "null (json)";
        }
        if (element.isJsonArray()) {
            return "an array (" + text + ")";
        }
        if (element.isJsonObject()) {
            return "an object (" + text + ")";
        }
        if (element.isJsonPrimitive()) {
            JsonPrimitive primitive = element.getAsJsonPrimitive();
            if (primitive.isNumber()) {
        return "a number (" + text + ")";
            }
            if (primitive.isBoolean()) {
        return "a boolean (" + text + ")";
            }
        }
        return text;
    }
    public static <T> T fromJson(Gson gson, Reader reader, Class<T> type) {
        return gson.fromJson(reader, type);
    }
    public static <T> T fromNullableJson(Gson gson, Reader reader, TypeToken<T> type) {
        try {
            JsonReader jsonReader = new JsonReader(reader);
            jsonReader.setStrictness(com.google.gson.Strictness.LENIENT);
            return gson.getAdapter(type).read(jsonReader);
        } catch (IOException e) {
            throw new JsonParseException(e);
        }
    }
    public static <T> T fromJson(Gson gson, Reader reader, TypeToken<T> type) {
        T value = fromNullableJson(gson, reader, type);
        if (value == null) {
            throw new JsonParseException("JSON data was null or empty");
        }
        return value;
    }
    public static <T> T fromNullableJson(Gson gson, String string, TypeToken<T> type) {
        return fromNullableJson(gson, new java.io.StringReader(string), type);
    }
    public static JsonObject parse(String string) {
        return parse(new java.io.StringReader(string));
    }
    public static JsonObject parse(Reader reader) {
        return fromJson(GSON, reader, JsonObject.class);
    }
    public static JsonArray parseArray(String string) {
        return parseArray(new java.io.StringReader(string));
    }
    public static JsonArray parseArray(Reader reader) {
        return fromJson(GSON, reader, JsonArray.class);
    }
    public static String toStableString(JsonElement jsonElement) {
        java.io.StringWriter writer = new java.io.StringWriter();
        JsonWriter out = new JsonWriter(writer);
        try {
            writeValue(out, jsonElement, Comparator.naturalOrder());
        } catch (IOException e) {
            throw new AssertionError(e);
        }
        return writer.toString();
    }
    public static void writeValue(JsonWriter out, JsonElement value, Comparator<String> keyComparator) throws IOException {
        if (value == null || value.isJsonNull()) {
            out.nullValue();
        } else if (value.isJsonPrimitive()) {
            JsonPrimitive primitive = value.getAsJsonPrimitive();
            if (primitive.isNumber()) {
        out.value(primitive.getAsNumber());
            } else if (primitive.isBoolean()) {
        out.value(primitive.getAsBoolean());
            } else {
        out.value(primitive.getAsString());
            }
        } else if (value.isJsonArray()) {
            out.beginArray();
            for (JsonElement element : value.getAsJsonArray()) {
        writeValue(out, element, keyComparator);
            }
            out.endArray();
        } else if (value.isJsonObject()) {
            out.beginObject();
            java.util.List<Map.Entry<String, JsonElement>> entries = new java.util.ArrayList<>(value.getAsJsonObject().entrySet());
            entries.sort(Map.Entry.comparingByKey(keyComparator));
            for (Map.Entry<String, JsonElement> entry : entries) {
        out.name(entry.getKey());
        writeValue(out, entry.getValue(), keyComparator);
            }
            out.endObject();
        } else {
            throw new IllegalArgumentException("Couldn't write " + value.getClass());
        }
    }
    public static boolean encodesLongerThan(JsonElement element, int limit) {
        return toStableString(element).length() > limit;
    }
    static class CountedAppendable implements Appendable {
        private final int limit;
        private int count;

        public CountedAppendable(int limit) {
            this.limit = limit;
        }

        private Appendable add(int length) {
            count += length;
            if (count > limit) {
                throw new IllegalStateException("Appendable limit of " + limit + " exceeded");
            }
            return this;
        }

        public Appendable append(CharSequence csq) {
            return add(csq == null ? 4 : csq.length());
        }

        public Appendable append(CharSequence csq, int start, int end) {
            return add(end - start);
        }

        public Appendable append(char c) {
            return add(1);
        }
    }
}
