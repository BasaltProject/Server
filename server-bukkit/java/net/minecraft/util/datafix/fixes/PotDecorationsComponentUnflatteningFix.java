// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class PotDecorationsComponentUnflatteningFix extends DataFix {
    public PotDecorationsComponentUnflatteningFix(Schema outputSchema) {
        super(outputSchema, true);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static <T> Dynamic<T> unpackList(Dynamic<T> original) {
        java.util.Optional<java.util.stream.Stream<Dynamic<T>>> items = original.asStreamOpt().result();
        if (items.isEmpty()) {
            return original;
        }
        java.util.List<String> ids = items.get().map(item -> item.asString().result().orElse("minecraft:brick")).toList();
        String[] sides = {"back", "left", "right", "front"};
        java.util.Map<Dynamic<T>, Dynamic<T>> out = new java.util.HashMap<>();
        for (int i = 0; i < sides.length; i++) {
            String id = i < ids.size() ? ids.get(i) : "minecraft:brick";
            if (id.isEmpty()) {
                return original;
            }
            out.put(original.createString(sides[i]), original.createMap(java.util.Map.of(original.createString("id"), original.createString(id))));
        }
        return original.createMap(out);
    }
}
