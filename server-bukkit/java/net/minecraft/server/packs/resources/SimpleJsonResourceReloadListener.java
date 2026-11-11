// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import com.mojang.serialization.JsonOps;
import net.minecraft.util.StrictJsonParser;
import com.google.gson.JsonElement;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import java.util.Map;
import net.minecraft.resources.FileToIdConverter;
import net.minecraft.resources.Identifier;
import net.minecraft.util.profiling.ProfilerFiller;
public abstract class SimpleJsonResourceReloadListener<T> extends SimplePreparableReloadListener<Map<Identifier, T>> {
    private final DynamicOps<JsonElement> ops;
    private final Codec<T> codec;
    private final FileToIdConverter lister;

    protected SimpleJsonResourceReloadListener(Codec<T> codec, FileToIdConverter lister) {
        this(JsonOps.INSTANCE, codec, lister);
        
    }
    private SimpleJsonResourceReloadListener(DynamicOps<JsonElement> ops, Codec<T> codec, FileToIdConverter lister) {
        this.ops = ops;
        this.codec = codec;
        this.lister = lister;
        
    }
    protected Map<Identifier, T> prepare(ResourceManager manager, ProfilerFiller profiler) {
        Map<Identifier, T> parsed = new java.util.HashMap<>();
        for (Map.Entry<Identifier, Resource> entry : lister.listMatchingResources(manager).entrySet()) {
            Identifier id = lister.fileToId(entry.getKey());
            try (java.io.Reader reader = entry.getValue().openAsReader()) {
                codec.parse(ops, StrictJsonParser.parse(reader)).ifSuccess(value -> {
                    if (parsed.putIfAbsent(id, value) != null) {
                        throw new IllegalStateException("Duplicate data file ignored with ID " + id);
                    }
                });
            } catch (IllegalArgumentException | java.io.IOException | com.google.gson.JsonParseException failure) {
                continue;
            }
        }
        return parsed;
    }
}
