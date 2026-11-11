// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import com.google.common.hash.Hashing;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.mojang.serialization.Codec;
import com.mojang.serialization.JsonOps;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.CompletableFuture;
import java.util.function.Function;
import java.util.function.ToIntFunction;
import net.minecraft.core.HolderLookup;
import net.minecraft.resources.Identifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public interface DataProvider {
    ToIntFunction<String> FIXED_ORDER_FIELDS = key -> switch (key) {
        case "type" -> 0;
        case "parent" -> 1;
        case "values" -> 2;
        case "pools" -> 3;
        default -> 2147483647;
    };
    Comparator<String> KEY_COMPARATOR = Comparator.comparingInt(FIXED_ORDER_FIELDS).thenComparing(key -> key);
    Logger LOGGER = LoggerFactory.getLogger(DataProvider.class);

    CompletableFuture<?> run(CachedOutput cache);

    String getName();

    static <T> CompletableFuture<?> saveAll(CachedOutput cache, Codec<T> codec, PackOutput.PathProvider pathProvider, Map<Identifier, T> entries) {
        return saveAll(cache, codec, pathProvider::json, entries);
    }

    static <T, E> CompletableFuture<?> saveAll(CachedOutput cache, Codec<E> codec, Function<T, Path> pathGetter, Map<T, E> contents) {
        return saveAll(cache, value -> codec.encodeStart(JsonOps.INSTANCE, value).getOrThrow(), pathGetter, contents);
    }

    static <T, E> CompletableFuture<?> saveAll(CachedOutput cache, Function<E, JsonElement> serializer, Function<T, Path> pathGetter, Map<T, E> contents) {
        List<CompletableFuture<?>> writes = new ArrayList<>();
        for (Map.Entry<T, E> entry : contents.entrySet()) {
            writes.add(saveStable(cache, serializer.apply(entry.getValue()), pathGetter.apply(entry.getKey())));
        }
        return CompletableFuture.allOf(writes.toArray(CompletableFuture[]::new));
    }

    static <T> CompletableFuture<?> saveStable(CachedOutput cache, HolderLookup.Provider registries, Codec<T> codec, T value, Path path) {
        return saveStable(cache, codec.encodeStart(registries.createSerializationContext(JsonOps.INSTANCE), value).getOrThrow(), path);
    }

    static <T> CompletableFuture<?> saveStable(CachedOutput cache, Codec<T> codec, T value, Path path) {
        return saveStable(cache, codec.encodeStart(JsonOps.INSTANCE, value).getOrThrow(), path);
    }

    static CompletableFuture<?> saveStable(CachedOutput cache, JsonElement root, Path path) {
        return CompletableFuture.runAsync(() -> {
            try {
                byte[] bytes = (GSON.toJson(sorted(root)) + "\n").getBytes(StandardCharsets.UTF_8);
                cache.writeIfNeeded(path, bytes, Hashing.sha1().hashBytes(bytes));
            } catch (IOException e) {
                LOGGER.error("Failed to save file to {}", path, e);
            }
        });
    }

    Gson GSON = new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create();

    private static JsonElement sorted(JsonElement element) {
        if (element.isJsonObject()) {
            TreeMap<String, JsonElement> ordered = new TreeMap<>(KEY_COMPARATOR);
            element.getAsJsonObject().entrySet().forEach(entry -> ordered.put(entry.getKey(), sorted(entry.getValue())));
            JsonObject out = new JsonObject();
            ordered.forEach(out::add);
            return out;
        }
        if (element.isJsonArray()) {
            com.google.gson.JsonArray out = new com.google.gson.JsonArray();
            element.getAsJsonArray().forEach(child -> out.add(sorted(child)));
            return out;
        }
        return element;
    }

    public interface Factory<T extends DataProvider> {
        T create(PackOutput output);
    }
}
