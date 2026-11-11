// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.Hook;
import com.mojang.datafixers.types.templates.TypeTemplate;
import java.util.Map;
import java.util.function.Supplier;
public class V1451_6 extends NamespacedSchema {
    public static final String SPECIAL_OBJECTIVE_MARKER = "_special";
    protected static final Hook.HookFunction UNPACK_OBJECTIVE_ID = new Hook.HookFunction() {
        public <T> T apply(com.mojang.serialization.DynamicOps<T> ops, T value) {
            com.mojang.serialization.Dynamic<T> input = new com.mojang.serialization.Dynamic<>(ops, value);
            java.util.Optional<String> criteria = input.get("CriteriaName").asString().result();
            if (criteria.isEmpty()) {
                return value;
            }
            String name = criteria.get();
            int colon = name.indexOf(':');
            String type = "_special";
            String id = name;
            if (colon >= 0) {
                try {
                    type = net.minecraft.resources.Identifier.bySeparator(name.substring(0, colon), '.').toString();
                    id = net.minecraft.resources.Identifier.bySeparator(name.substring(colon + 1), '.').toString();
                } catch (RuntimeException invalid) {
                    type = "_special";
                    id = name;
                }
            }
            return input.set("CriteriaType", input.createMap(Map.of(input.createString("type"), input.createString(type),
                input.createString("id"), input.createString(id)))).remove("CriteriaName").getValue();
        }
    };
    protected static final Hook.HookFunction REPACK_OBJECTIVE_ID = new Hook.HookFunction() {
        public <T> T apply(com.mojang.serialization.DynamicOps<T> ops, T value) {
            com.mojang.serialization.Dynamic<T> input = new com.mojang.serialization.Dynamic<>(ops, value);
            java.util.Optional<com.mojang.serialization.Dynamic<T>> criteria = input.get("CriteriaType").get().result();
            if (criteria.isEmpty()) {
                return value;
            }
            java.util.Optional<String> type = criteria.get().get("type").asString().result();
            java.util.Optional<String> id = criteria.get().get("id").asString().result();
            if (type.isEmpty() || id.isEmpty()) {
                return value;
            }
            String packed = type.get().equals("_special") ? id.get()
                : packNamespacedWithDot(type.get()) + ":" + packNamespacedWithDot(id.get());
            return input.set("CriteriaName", input.createString(packed)).remove("CriteriaType").getValue();
        }
    };
    public V1451_6(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
    protected static Map<String, Supplier<TypeTemplate>> createCriterionTypes(Schema schema) {
        return new java.util.HashMap<>();
    }
    public static String packNamespacedWithDot(String location) {
        net.minecraft.resources.Identifier parsed = net.minecraft.resources.Identifier.tryParse(location);
        return parsed != null ? parsed.getNamespace() + "." + parsed.getPath() : location;
    }
}
