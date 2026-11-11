// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.function.Supplier;
public class V3439 extends NamespacedSchema {
    public V3439(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public Map<String, Supplier<TypeTemplate>> registerBlockEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerBlockEntities(schema);
    }
    public static TypeTemplate sign(Schema schema) {
        return com.mojang.datafixers.DSL.remainder();
    }
}
