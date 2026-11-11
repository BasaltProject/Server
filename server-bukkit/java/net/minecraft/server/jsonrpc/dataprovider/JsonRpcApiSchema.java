// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.dataprovider;

import com.mojang.serialization.JsonOps;
import java.nio.file.Path;
import java.util.concurrent.CompletableFuture;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.server.jsonrpc.api.Schema;
import net.minecraft.server.jsonrpc.methods.DiscoveryService;

public class JsonRpcApiSchema implements DataProvider {
    private final Path target;

    public JsonRpcApiSchema(PackOutput packOutput) {
        target = packOutput.getOutputFolder(PackOutput.Target.REPORTS).resolve("json-rpc-api-schema.json");
    }

    public CompletableFuture<?> run(CachedOutput cache) {
        var document = DiscoveryService.DiscoverResponse.CODEC.codec()
            .encodeStart(JsonOps.INSTANCE, DiscoveryService.discover(Schema.getSchemaRegistry())).getOrThrow();
        return DataProvider.saveStable(cache, document, target);
    }

    public String getName() {
        return "Json RPC API schema";
    }
}
