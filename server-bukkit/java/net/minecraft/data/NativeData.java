// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.CompletableFuture;
import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.LeftoverNatives;

/** The enabled data packs of the running server, read through the native pack service. */
public final class NativeData {
    private static final String kNamespace = "minecraft";

    private NativeData() {
    }

    public static Map<Identifier, JsonElement> json(String folder) {
        return json("data/" + kNamespace + "/", folder);
    }

    public static Map<Identifier, JsonElement> json(String packRoot, String folder) {
        String prefix = packRoot + folder + "/";
        Map<Identifier, JsonElement> found = new TreeMap<>();
        for (String name : LeftoverNatives.resourcesUnder(prefix)) {
            if (!name.endsWith(".json")) {
                continue;
            }
            byte[] bytes = LeftoverNatives.resourceRead(name);
            if (bytes != null) {
                String path = name.substring(prefix.length(), name.length() - ".json".length());
                found.put(Identifier.fromNamespaceAndPath(kNamespace, path), JsonParser.parseString(new String(bytes, StandardCharsets.UTF_8)));
            }
        }
        return found;
    }

    public static List<String> names(String prefix) {
        return new ArrayList<>(List.of(LeftoverNatives.resourcesUnder(prefix)));
    }

    public static byte[] read(String name) {
        return LeftoverNatives.resourceRead(name);
    }

    public static CompletableFuture<?> copyFolder(CachedOutput cache, PackOutput output, String folder) {
        PackOutput.PathProvider paths = output.createPathProvider(PackOutput.Target.DATA_PACK, folder);
        return DataProvider.saveAll(cache, element -> element, paths::json, json(folder));
    }
}
