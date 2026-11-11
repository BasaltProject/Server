// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;
import com.google.gson.JsonObject;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.Registry;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.StringRepresentable;
public class DatapackStructureReport implements DataProvider {
    private final PackOutput output;

    public DatapackStructureReport(PackOutput output) {
        this.output = output;
    }
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject registries = new JsonObject();
        for (var data : net.minecraft.resources.RegistryDataLoader.WORLD_REGISTRIES) {
            JsonObject entry = new JsonObject();
            entry.addProperty("elements", true);
            entry.addProperty("tags", true);
            entry.addProperty("stable", true);
            registries.add(data.key().identifier().toString(), entry);
        }
        JsonObject root = new JsonObject();
        root.add("registries", registries);
        return ReportJson.write(cache, output, "datapack.json", root);
    }
    public String getName() {
        return "Datapack Structure";
    }
    static record Report(Map<ResourceKey<? extends Registry<?>>, Entry> registries, Map<String, CustomPackEntry> others) {
        public static final Codec<Report> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.data.info.DatapackStructureReport.Report.class, "CODEC");
    }
    static record Entry(boolean elements, boolean tags, boolean stable) {
        public static final MapCodec<Entry> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.data.info.DatapackStructureReport.Entry.class, "MAP_CODEC");
        public static final Codec<Entry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.data.info.DatapackStructureReport.Entry.class, "CODEC");
    }
    static enum Format implements StringRepresentable {
        STRUCTURE, MCFUNCTION;
        public static final Codec<Format> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.data.info.DatapackStructureReport.Format.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    static record CustomPackEntry(Format format, Entry entry) {
        public static final Codec<CustomPackEntry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.data.info.DatapackStructureReport.CustomPackEntry.class, "CODEC");
    }
}
