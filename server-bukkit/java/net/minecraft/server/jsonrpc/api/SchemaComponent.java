// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.api;

import java.net.URI;
import java.util.List;

public record SchemaComponent<T>(String name, URI ref, Schema<T> schema) {
    public Schema<T> asRef() {
        return Schema.ofRef(ref, schema.codec());
    }

    public Schema<List<T>> asArray() {
        return Schema.arrayOf(asRef(), schema.codec());
    }
}
