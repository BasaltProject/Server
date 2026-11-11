// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.locale;
import com.google.gson.JsonElement;
import com.mojang.serialization.Codec;
import com.mojang.serialization.JsonOps;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Map;
import net.minecraft.util.StrictJsonParser;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public record DeprecatedTranslationsInfo(List<String> removed, Map<String, String> renamed) {
    private static final Logger kLog = LoggerFactory.getLogger(DeprecatedTranslationsInfo.class);
    public static final DeprecatedTranslationsInfo EMPTY = new DeprecatedTranslationsInfo(List.of(), Map.of());
    public static final Codec<DeprecatedTranslationsInfo> CODEC = RecordCodecBuilder.create(i -> i.group(
            Codec.STRING.listOf().fieldOf("removed").forGetter(DeprecatedTranslationsInfo::removed),
            Codec.unboundedMap(Codec.STRING, Codec.STRING).fieldOf("renamed").forGetter(DeprecatedTranslationsInfo::renamed)).apply(i, DeprecatedTranslationsInfo::new));
    public static DeprecatedTranslationsInfo loadFromJson(InputStream stream) {
        JsonElement json = StrictJsonParser.parse(new InputStreamReader(stream, StandardCharsets.UTF_8));
        return CODEC.parse(JsonOps.INSTANCE, json).getOrThrow(msg -> new IllegalStateException("Failed to parse deprecated language data: " + msg));
    }
    public static DeprecatedTranslationsInfo loadFromResource(String path) {
        try (InputStream stream = Language.class.getResourceAsStream(path)) {
            return stream == null ? EMPTY : loadFromJson(stream);
        } catch (Exception e) {
            kLog.error("Failed to read {}", path, e);
            return EMPTY;
        }
    }
    public static DeprecatedTranslationsInfo loadFromDefaultResource() {
        return loadFromResource("/assets/minecraft/lang/deprecated.json");
    }
    public void applyToMap(Map<String, String> translations) {
        removed.forEach(translations::remove);
        renamed.forEach((from, to) -> {
            String value = translations.remove(from);
            if (value == null) {
                kLog.warn("Missing translation key for rename: {}", from);
                translations.remove(to);
            } else {
                translations.put(to, value);
            }
        });
    }
}
