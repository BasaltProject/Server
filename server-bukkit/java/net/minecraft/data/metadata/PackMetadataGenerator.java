// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.metadata;

import com.google.gson.JsonObject;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.network.chat.Component;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.world.flag.FeatureFlagSet;

public class PackMetadataGenerator implements DataProvider {
    private final PackOutput output;
    private final Map<String, com.google.gson.JsonElement> sections = new LinkedHashMap<>();

    public PackMetadataGenerator(PackOutput output) {
        this.output = output;
    }

    public <T> PackMetadataGenerator add(MetadataSectionType<T> type, T value) {
        com.google.gson.JsonElement json = type.codec().encodeStart(com.mojang.serialization.JsonOps.INSTANCE, value).getOrThrow();
        sections.put(type.name(), json);
        return this;
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject root = new JsonObject();
        sections.forEach(root::add);
        return DataProvider.saveStable(cache, root, output.getOutputFolder().resolve("pack.mcmeta"));
    }

    @Override
    public final String getName() {
        return "Pack Metadata";
    }

    public static PackMetadataGenerator forFeaturePack(PackOutput output, Component description) {
        return forFeaturePack(output, description, FeatureFlagSet.of());
    }

    public static PackMetadataGenerator forFeaturePack(PackOutput output, Component description, FeatureFlagSet flags) {
        PackMetadataGenerator generator = new PackMetadataGenerator(output);
        JsonObject pack = new JsonObject();
        pack.addProperty("description", description.getString());
        generator.sections.put("pack", pack);
        return generator;
    }
}
