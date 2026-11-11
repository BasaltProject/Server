// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class WrittenBookPagesStrictJsonFix extends ItemStackTagFix {
    public WrittenBookPagesStrictJsonFix(Schema outputSchema) {
        super(outputSchema, "WrittenBookPagesStrictJsonFix", (java.util.function.Predicate) null);
    }
    protected Typed<?> fixItemStackTag(Typed<?> tag) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("item_stack", getClass().getSimpleName(), getVersionKey(), tag);
    }
}
