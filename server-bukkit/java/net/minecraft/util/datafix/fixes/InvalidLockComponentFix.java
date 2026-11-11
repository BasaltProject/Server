// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class InvalidLockComponentFix extends DataComponentRemainderFix {
    public InvalidLockComponentFix(Schema outputSchema) {
        super(outputSchema, "InvalidLockComponentPredicateFix", "minecraft:lock");
    }
    protected <T> Dynamic<T> fixComponent(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("item_stack", getClass().getSimpleName(), getVersionKey(), input);
    }
    public static <T> Dynamic<T> fixLock(Dynamic<T> input) {
        java.util.Optional<String> blank = java.util.Optional.of("\"\"");
        java.util.Optional<java.util.Map<Dynamic<T>, Dynamic<T>>> outer = input.getMapValues().result();
        if (outer.isEmpty() || outer.get().size() != 1) {
            return input;
        }
        Dynamic<T> components = input.get("components").result().orElse(null);
        java.util.Optional<java.util.Map<Dynamic<T>, Dynamic<T>>> inner = components == null ? java.util.Optional.empty() : components.getMapValues().result();
        boolean broken = inner.isPresent() && inner.get().size() == 1 && components.get("minecraft:custom_name").asString().result().equals(blank);
        return broken ? null : input;
    }
}
