// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import java.util.Optional;
import net.minecraft.util.GsonHelper;
import net.minecraft.util.LenientJsonParser;
import net.minecraft.util.StrictJsonParser;
public class LegacyComponentDataFixUtils {
    private static final String kEmpty = createTextComponentJson("");
    private static String single(String field, String value) {
        JsonObject object = new JsonObject();
        object.addProperty(field, value);
        return GsonHelper.toStableString(object);
    }
    public static <T> Dynamic<T> createPlainTextComponent(DynamicOps<T> ops, String text) {
        return new Dynamic<>(ops, ops.createString(createTextComponentJson(text)));
    }
    public static <T> Dynamic<T> createEmptyComponent(DynamicOps<T> ops) {
        return new Dynamic<>(ops, ops.createString(kEmpty));
    }
    public static String createTextComponentJson(String text) {
        return single("text", text);
    }
    public static String createTranslatableComponentJson(String key) {
        return single("translate", key);
    }
    public static <T> Dynamic<T> createTranslatableComponent(DynamicOps<T> ops, String key) {
        return new Dynamic<>(ops, ops.createString(createTranslatableComponentJson(key)));
    }
    public static String rewriteFromLenient(String string) {
        if (string.isEmpty() || string.equals("null")) {
            return kEmpty;
        }
        char open = string.charAt(0);
        char close = string.charAt(string.length() - 1);
        boolean looksLikeJson = (open == '"' && close == '"') || (open == '{' && close == '}') || (open == '[' && close == ']');
        if (looksLikeJson) {
            try {
                JsonElement json = LenientJsonParser.parse(string);
                return json.isJsonPrimitive() ? createTextComponentJson(json.getAsString()) : GsonHelper.toStableString(json);
            } catch (JsonParseException ignored) {
                // fall through to plain text
            }
        }
        return createTextComponentJson(string);
    }
    public static boolean isStrictlyValidJson(Dynamic<?> component) {
        return component.asString().result().filter(text -> {
            try {
                StrictJsonParser.parse(text);
                return true;
            } catch (JsonParseException e) {
                return false;
            }
        }).isPresent();
    }
    public static Optional<String> extractTranslationString(String component) {
        try {
            JsonElement parsed = LenientJsonParser.parse(component);
            if (parsed.isJsonObject()) {
                JsonElement key = parsed.getAsJsonObject().get("translate");
                if (key != null && key.isJsonPrimitive()) {
                    return Optional.of(key.getAsString());
                }
            }
        } catch (JsonParseException ignored) {
            // not a component
        }
        return Optional.empty();
    }
}
