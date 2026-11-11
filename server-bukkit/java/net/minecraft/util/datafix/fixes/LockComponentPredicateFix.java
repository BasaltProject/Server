// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.google.common.escape.Escaper;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class LockComponentPredicateFix extends DataComponentRemainderFix {
    public static final Escaper ESCAPER = com.google.common.escape.Escapers.builder().addEscape('"', "\\\"").addEscape('\\', "\\\\").build();
    public LockComponentPredicateFix(Schema outputSchema) {
        super(outputSchema, "LockComponentPredicateFix", "minecraft:lock");
    }
    protected <T> Dynamic<T> fixComponent(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("item_stack", getClass().getSimpleName(), getVersionKey(), input);
    }
    public static <T> Dynamic<T> fixLock(Dynamic<T> input) {
        String name = input.asString().result().orElse("");
        if (name.isEmpty()) {
            return null;
        }
        Dynamic<T> named = input.createString("\"" + ESCAPER.escape(name) + "\"");
        return input.emptyMap().set("components", input.emptyMap().set("minecraft:custom_name", named));
    }
}
