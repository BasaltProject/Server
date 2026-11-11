// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.api;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.net.URI;

public class ReferenceUtil {
    public static final Codec<URI> REFERENCE_CODEC = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(URI.create(text));
        } catch (IllegalArgumentException bad) {
            return DataResult.error(bad::getMessage);
        }
    }, URI::toString);

    public static URI createLocalReference(String typeId) {
        return URI.create("#/components/schemas/" + typeId);
    }
}
