// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.function.Supplier;
public class V1906 extends NamespacedSchema {
    public V1906(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public Map<String, Supplier<TypeTemplate>> registerBlockEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerBlockEntities(schema);
    }
    protected static void registerInventory(Schema schema, Map<String, Supplier<TypeTemplate>> map, String name) {
        schema.register(map, name, (java.util.function.Supplier<TypeTemplate>) com.mojang.datafixers.DSL::remainder);
    }
}
