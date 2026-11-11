// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;
import net.minecraft.data.NativeData;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
public class BiomeParametersDumpReport implements DataProvider {
    private final PackOutput output;

    public BiomeParametersDumpReport(PackOutput output, CompletableFuture<HolderLookup.Provider> registries) {
        this.output = output;
    }
    public CompletableFuture<?> run(CachedOutput cache) {
        return NativeData.copyFolder(cache, output, "worldgen/multi_noise_biome_source_parameter_list");
    }
    public final String getName() {
        return "Biome Parameters";
    }
}
