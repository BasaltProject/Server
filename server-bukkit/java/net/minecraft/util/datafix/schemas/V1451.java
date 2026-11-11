// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.function.Supplier;
public class V1451 extends NamespacedSchema {
    public V1451(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public Map<String, Supplier<TypeTemplate>> registerBlockEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerBlockEntities(schema);
    }
}
