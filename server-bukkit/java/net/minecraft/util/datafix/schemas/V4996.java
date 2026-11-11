// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.SequencedMap;
import java.util.function.Supplier;
public class V4996 extends NamespacedSchema {
    public V4996(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public static SequencedMap<String, Supplier<TypeTemplate>> components(Schema schema) {
        return new java.util.LinkedHashMap<>();
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
}
