// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;

import com.google.gson.JsonElement;
import java.util.concurrent.CompletableFuture;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;

final class ReportJson {
    private ReportJson() {
    }

    static CompletableFuture<?> write(CachedOutput cache, PackOutput output, String file, JsonElement json) {
        return DataProvider.saveStable(cache, json, output.getOutputFolder(PackOutput.Target.REPORTS).resolve(file));
    }
}
