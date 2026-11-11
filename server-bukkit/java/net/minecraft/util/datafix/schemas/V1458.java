// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.function.Supplier;
public class V1458 extends NamespacedSchema {
    public V1458(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
    public Map<String, Supplier<TypeTemplate>> registerBlockEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerBlockEntities(schema);
    }
    public static TypeTemplate nameableInventory(Schema schema) {
        return com.mojang.datafixers.DSL.remainder();
    }
    public static TypeTemplate nameable(Schema schema) {
        return com.mojang.datafixers.DSL.remainder();
    }
}
