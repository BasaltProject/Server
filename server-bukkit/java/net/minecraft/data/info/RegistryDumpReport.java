// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;

import com.google.gson.JsonObject;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.DefaultedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.resources.Identifier;

public class RegistryDumpReport implements DataProvider {
    private final PackOutput output;

    public RegistryDumpReport(PackOutput output) {
        this.output = output;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject root = new JsonObject();
        for (Field field : BuiltInRegistries.class.getFields()) {
            if (!Modifier.isStatic(field.getModifiers()) || !Registry.class.isAssignableFrom(field.getType())) {
                continue;
            }
            try {
                Registry registry = (Registry) field.get(null);
                if (registry == null || registry == BuiltInRegistries.REGISTRY) {
                    continue;
                }
                JsonObject entries = new JsonObject();
                for (Object value : registry) {
                    JsonObject entry = new JsonObject();
                    entry.addProperty("protocol_id", registry.getId(value));
                    entries.add(registry.getKey(value).toString(), entry);
                }
                JsonObject dump = new JsonObject();
                if (registry instanceof DefaultedRegistry<?> defaulted) {
                    dump.addProperty("default", defaulted.getDefaultKey().toString());
                }
                dump.addProperty("protocol_id", root.size());
                dump.add("entries", entries);
                root.add(registry.key().identifier().toString(), dump);
            } catch (IllegalAccessException | RuntimeException ignored) {
                continue;
            }
        }
        return ReportJson.write(cache, output, "registries.json", root);
    }

    @Override
    public final String getName() {
        return "Registry Dump";
    }
}
