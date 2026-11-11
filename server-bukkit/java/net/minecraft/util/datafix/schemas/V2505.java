// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.function.Supplier;
public class V2505 extends NamespacedSchema {
    public V2505(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public Map<String, Supplier<TypeTemplate>> registerEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerEntities(schema);
    }
}
