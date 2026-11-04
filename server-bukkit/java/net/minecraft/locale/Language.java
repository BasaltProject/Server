// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.locale;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;
import java.util.stream.Stream;
import net.minecraft.network.chat.FormattedText;
import net.minecraft.util.FormattedCharSequence;

public abstract class Language {
    public static final String DEFAULT = "en_us";
    public static final Language DEFAULT_INSTANCE = new Language() {
        private volatile Map<String, String> translations;

        private Map<String, String> table() {
            Map<String, String> loaded = translations;
            if (loaded == null) {
                synchronized (this) {
                    if (translations == null) {
                        translations = loadFromAssetsCache();
                    }
                    loaded = translations;
                }
            }
            return loaded;
        }

        @Override
        public String getOrDefault(String elementId, String defaultValue) {
            return table().getOrDefault(elementId, defaultValue);
        }

        @Override
        public boolean has(String elementId) {
            return table().containsKey(elementId);
        }

        @Override
        public boolean isDefaultRightToLeft() {
            return false;
        }

        @Override
        public FormattedCharSequence getVisualOrder(FormattedText logicalOrderText) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.locale.Language#getVisualOrder");
        }
    };
    private static volatile Language instance = DEFAULT_INSTANCE;

    private static Map<String, String> loadFromAssetsCache() {
        Map<String, String> table = new HashMap<>();
        Path root = Path.of(".basalt", "assets");
        if (!Files.isDirectory(root)) {
            return table;
        }
        try (Stream<Path> versions = Files.list(root)) {
            List<Path> candidates = new ArrayList<>(versions.sorted().toList());
            for (Path version : candidates) {
                Path file = version.resolve("assets/minecraft/lang/" + DEFAULT + ".json");
                if (Files.isRegularFile(file)) {
                    try (InputStream stream = Files.newInputStream(file)) {
                        loadFromJson(stream, table::put);
                    }
                    break;
                }
            }
        } catch (IOException e) {
            return table;
        }
        return table;
    }

    public static void loadFromJson(InputStream stream, BiConsumer<String, String> output) {
        JsonObject object = JsonParser.parseReader(new InputStreamReader(stream, StandardCharsets.UTF_8)).getAsJsonObject();
        for (Map.Entry<String, JsonElement> entry : object.entrySet()) {
            if (entry.getValue().isJsonPrimitive()) {
                output.accept(entry.getKey(), entry.getValue().getAsString());
            }
        }
    }

    public static Language getInstance() {
        return instance;
    }

    public static void inject(Language language) {
        instance = language;
    }

    public String getOrDefault(String elementId) {
        return getOrDefault(elementId, elementId);
    }

    public abstract String getOrDefault(String elementId, String defaultValue);

    public abstract boolean has(String elementId);

    public abstract boolean isDefaultRightToLeft();

    public abstract FormattedCharSequence getVisualOrder(FormattedText logicalOrderText);

    public List<FormattedCharSequence> getVisualOrder(List<FormattedText> lines) {
        return lines.stream().map(this::getVisualOrder).toList();
    }
}
