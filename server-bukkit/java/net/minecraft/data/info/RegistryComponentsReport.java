// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;
import com.google.gson.JsonObject;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
public class RegistryComponentsReport implements DataProvider {
    private final PackOutput output;

    public RegistryComponentsReport(PackOutput output, CompletableFuture<HolderLookup.Provider> registries) {
        this.output = output;
    }
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject root = new JsonObject();
        for (net.minecraft.world.item.Item item : net.minecraft.core.registries.BuiltInRegistries.ITEM) {
            JsonObject components = new JsonObject();
            for (net.minecraft.core.component.DataComponentType<?> type : item.components().keySet()) {
                components.addProperty(String.valueOf(net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_TYPE.getKey(type)), true);
            }
            root.add(net.minecraft.core.registries.BuiltInRegistries.ITEM.getKey(item).toString(), components);
        }
        return ReportJson.write(cache, output, "item_components.json", root);
    }
    public final String getName() {
        return "Registry Components";
    }
}
