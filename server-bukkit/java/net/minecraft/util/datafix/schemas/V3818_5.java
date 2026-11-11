// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.Hook;
import com.mojang.datafixers.types.templates.TypeTemplate;
import com.mojang.datafixers.types.templates.Hook.HookFunction;
import java.util.Map;
import java.util.function.Supplier;
public class V3818_5 extends NamespacedSchema {
    protected static final Hook.HookFunction UNPACK_PLAIN_ID = new Hook.HookFunction() {
        public <T> T apply(com.mojang.serialization.DynamicOps<T> ops, T value) {
            return ops.getStringValue(value).result().isPresent()
                ? ops.createMap(Map.of(ops.createString("id"), value, ops.createString("count"), ops.createInt(1))) : value;
        }
    };
    public V3818_5(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
}
