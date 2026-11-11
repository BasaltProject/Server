// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class FireResistantToDamageResistantComponentFix extends DataComponentRemainderFix {
    public FireResistantToDamageResistantComponentFix(Schema outputSchema) {
        super(outputSchema, "FireResistantToDamageResistantComponentFix", "FireResistantToDamageResistantComponentFix");
    }
    protected <T> Dynamic<T> fixComponent(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("item_stack", getClass().getSimpleName(), getVersionKey(), input);
    }
}
